#include "CollabLive.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/CloudService.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>

#include <Geode/loader/Dirs.hpp>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <regex>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>

using namespace geode::prelude;

namespace geobr::collab {
namespace {

constexpr char const* UidKey = "geobr-collab-uid";
constexpr size_t MaxOpsPerRound = 1500;
constexpr char const* SettingsUid = "__settings";  // the level's music, synced like an object (host / ADMs only)
std::unordered_map<std::string, bool> s_canSave;

std::string uidOf(GameObject* object) {
    if (auto value = typeinfo_cast<CCString*>(object->getUserObject(UidKey))) return value->getCString();
    return {};
}

void setUid(GameObject* object, std::string const& uid) { object->setUserObject(UidKey, CCString::create(uid)); }

std::string randomPrefix() {
    static constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device device;
    std::mt19937 rng(device());
    std::string out;
    for (int i = 0; i < 6; ++i) out += alphabet[rng() % 36];
    return out;
}

bool special(std::string const& uid) { return uid.starts_with("__"); }

// Object string compared with rounding (the level save and the live object
// strings can differ in decimals, e.g. rotation -23 vs -23.02) and key order.
std::string matchKey(std::string const& data) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        auto comma = data.find(',', start);
        parts.push_back(data.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    static std::regex const number(R"(-?\d+(\.\d+)?)");
    std::vector<std::string> pairs;
    for (size_t i = 0; i + 1 < parts.size(); i += 2) {
        auto value = parts[i + 1];
        if (std::regex_match(value, number)) value = fmt::format("{:.1f}", std::round(std::strtod(value.c_str(), nullptr) * 10.0) / 10.0 + 0.0);
        pairs.push_back(parts[i] + "=" + value);
    }
    std::sort(pairs.begin(), pairs.end());
    std::string key;
    for (auto const& pair : pairs) key += pair + ";";
    return key;
}

// ------------------------------------------------------------------ trace
// Each step of the live session goes to a small file, flushed line by line,
// so a crash leaves the last step on disk. On the next start, an unfinished
// trace (no END) and the tail of the previous Geode log are sent to the
// server for diagnosis (tokens removed), then the file is renamed.
std::filesystem::path tracePath() { return Mod::get()->getSaveDir() / "collab-trace.txt"; }
std::ofstream s_trace;
size_t s_traceLines = 0;

void trace(std::string const& line) {
    if (!s_trace.is_open()) s_trace.open(tracePath(), std::ios::app);
    if (!s_trace) return;
    if (++s_traceLines > 20000) {  // keep the file small
        s_trace.close();
        s_trace.open(tracePath(), std::ios::trunc);
        s_traceLines = 0;
        s_trace << "TRUNC" << std::endl;
    }
    s_trace << std::time(nullptr) << ' ' << line << std::endl;
}

void traceBegin(std::string const& room) {
    s_trace.close();
    s_trace.open(tracePath(), std::ios::trunc);
    s_traceLines = 0;
#if defined(GEODE_IS_WINDOWS)
    constexpr char const* platform = "win";
#elif defined(GEODE_IS_ANDROID)
    constexpr char const* platform = "android";
#else
    constexpr char const* platform = "other";
#endif
    trace(fmt::format("BEGIN {} {} {}", Mod::get()->getVersion().toVString(), platform, room));
}

void traceEnd() {
    trace("END");
    s_trace.close();
}

std::deque<std::string> tailOf(std::filesystem::path const& path, size_t count) {
    std::deque<std::string> lines;
    std::ifstream in(path);
    for (std::string line; std::getline(in, line);) {
        lines.push_back(line.size() > 600 ? line.substr(0, 600) + "..." : line);
        if (lines.size() > count) lines.pop_front();
    }
    return lines;
}

std::string redact(std::string text) {
    static std::regex const jwt(R"(eyJ[A-Za-z0-9_\-]+\.[A-Za-z0-9_\-]+\.[A-Za-z0-9_\-]+)");
    static std::regex const key(R"((apikey|authorization|password|token)(["']?\s*[:=]\s*)[^\s,;"']+)", std::regex::icase);
    text = std::regex_replace(text, jwt, "<jwt>");
    return std::regex_replace(text, key, "$1$2<oculto>");
}

std::filesystem::path previousGeodeLog() {
    std::error_code ec;
    std::vector<std::filesystem::directory_entry> logs;
    for (auto const& entry : std::filesystem::directory_iterator(dirs::getGeodeLogDir(), ec))
        if (entry.is_regular_file(ec) && entry.path().extension() == ".log") logs.push_back(entry);
    if (logs.size() < 2) return {};
    std::sort(logs.begin(), logs.end(), [](auto const& a, auto const& b) {
        std::error_code e1, e2;
        return a.last_write_time(e1) > b.last_write_time(e2);
    });
    return logs[1].path();  // [0] is this session's log
}

// Music of the level (lives in the level / level settings, not in an object).
std::string settingsOf(LevelEditorLayer* editor) {
    auto level = editor->m_level;
    auto settings = editor->m_levelSettings;
    if (!level || !settings) return {};
    return matjson::makeObject({
        {"a", level->m_audioTrack}, {"s", level->m_songID}, {"o", settings->m_songOffset},
        {"fi", settings->m_fadeIn}, {"fo", settings->m_fadeOut},
    }).dump(matjson::NO_INDENTATION);
}

// A popup (object settings, level settings, chat...) may hold pointers to objects.
bool popupOpen() {
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return false;
    for (auto child : CCArrayExt<CCNode*>(scene->getChildren())) if (typeinfo_cast<FLAlertLayer*>(child)) return true;
    return false;
}

std::vector<GameObject*> objectsOf(LevelEditorLayer* editor) {
    std::vector<GameObject*> out;
    if (editor && editor->m_objects)
        for (auto object : CCArrayExt<GameObject*>(editor->m_objects)) if (object) out.push_back(object);
    return out;
}

struct Op { std::string uid; std::optional<std::string> data; std::optional<std::string> previous; };

class LiveSession final : public CCNode {
    EditorUI* m_ui = nullptr;
    std::string m_room, m_prefix;
    int m_counter = 0;
    int64_t m_seq = 0;
    bool m_ready = false, m_busy = false, m_again = false;
    std::time_t m_busySince = 0, m_lastLoad = 0, m_startedAt = 0;
    std::unordered_map<std::string, std::string> m_known;  // uid -> object string the room has
    struct Cursor { CCNode* node = nullptr; CCPoint pos; };
public:
    ~LiveSession() override { traceEnd(); }
private:
    std::unordered_map<std::string, Cursor> m_cursors;      // member name -> tag (on the UI, over the level)
    CCNode* m_cursorLayer = nullptr;
    // Remote changes waiting until it is safe to apply them (playtest, the
    // object selected / in a popup here): uid -> latest data (nullopt = deleted).
    std::unordered_map<std::string, std::optional<std::string>> m_deferred;
    bool m_flushQueued = false;
    static constexpr size_t MaxAppliedPerFlush = 400;
    std::vector<Op> m_pending;
    CloudRequest m_request;
    CCLabelBMFont* m_status = nullptr;

public:
    static LiveSession* create(EditorUI* ui, std::string const& room) {
        auto node = new LiveSession;
        if (node->init(ui, room)) { node->autorelease(); return node; }
        delete node;
        return nullptr;
    }

    bool init(EditorUI* ui, std::string const& room) {
        if (!CCNode::init()) return false;
        m_ui = ui;
        m_room = room;
        m_prefix = randomPrefix();
        setID("geobr-collab-live");
        traceBegin(room);
        auto win = CCDirector::sharedDirector()->getWinSize();
        m_status = CCLabelBMFont::create("Collab ao vivo: conectando...", "chatFont.fnt");
        m_status->setScale(.55f);
        m_status->setColor({120, 255, 160});
        m_status->setPosition({win.width / 2.f, win.height - 40.f});
        m_status->setID("geobr-collab-live-status");
        // Everything this session shows lives inside this node (one child of
        // the EditorUI), so GD and other mods never meet unknown nodes elsewhere.
        m_cursorLayer = CCNode::create();
        m_cursorLayer->setID("geobr-collab-cursors");
        addChild(m_cursorLayer, 1);
        addChild(m_status, 2);
        schedule(schedule_selector(LiveSession::tick), 1.f);
        scheduleUpdate();
        m_startedAt = std::time(nullptr);  // the room loads once the editor has settled (see tick)
        return true;
    }

    LevelEditorLayer* editor() const { return m_ui ? m_ui->m_editorLayer : nullptr; }

    void status(std::string const& text) { if (m_status) m_status->setString(text.c_str()); }

    std::string newUid() { return fmt::format("{}.{}", m_prefix, ++m_counter); }

    bool canSave() const { auto it = s_canSave.find(m_room); return it != s_canSave.end() && it->second; }

    // The editor finished opening (transition over, running) and other mods had
    // a moment to set up their own editor state before the room changes objects.
    bool settled(std::time_t now) const {
        auto ed = editor();
        if (!ed || !m_ui || !ed->isRunning() || !m_ui->isRunning()) return false;
        if (ed->getParent() != CCDirector::sharedDirector()->getRunningScene()) return false;
        return now - m_startedAt >= 2;
    }

    bool playing() const { auto ed = editor(); return ed && ed->m_playbackMode != PlaybackMode::Not; }

    // Undo/redo entries can point at objects the room replaced: drop them.
    void clearHistory() {
        auto ed = editor();
        if (!ed) return;
        if (ed->m_undoObjects) ed->m_undoObjects->removeAllObjects();
        if (ed->m_redoObjects) ed->m_redoObjects->removeAllObjects();
    }

    void applySettings(std::string const& data) {
        auto ed = editor();
        if (!ed || !ed->m_level || !ed->m_levelSettings) return;
        auto parsed = matjson::parse(data);
        if (!parsed) return;
        auto const& v = parsed.unwrap();
        auto level = ed->m_level;
        auto settings = ed->m_levelSettings;
        log::info("Collab ao vivo: musica da sala {}", data);
        level->m_audioTrack = static_cast<int>(jsonutil::integerOr(v, "a", level->m_audioTrack));
        level->m_songID = static_cast<int>(jsonutil::integerOr(v, "s", level->m_songID));
        settings->m_songOffset = static_cast<float>(v["o"].asDouble().unwrapOr(settings->m_songOffset));
        settings->m_fadeIn = jsonutil::boolOr(v, "fi", settings->m_fadeIn);
        settings->m_fadeOut = jsonutil::boolOr(v, "fo", settings->m_fadeOut);
        m_known[SettingsUid] = settingsOf(ed);
    }

    // Object changes are made at the start of a frame (like a touch), never in
    // the middle of the editor's own per-frame updates.
    void queueFlush() {
        if (m_flushQueued || m_deferred.empty()) return;
        m_flushQueued = true;
        WeakRef<LiveSession> weak = this;
        Loader::get()->queueInMainThread([weak] {
            if (auto self = weak.lock()) { self->m_flushQueued = false; self->flushDeferred(); }
        });
    }

    // ------------------------------------------------------------- start
    void load() {
        m_busy = true;
        m_busySince = m_lastLoad = std::time(nullptr);
        m_request.rpc("geobr_collab_live_load", matjson::makeObject({{"p_room", m_room}}), [this](auto r) {
            m_busy = false;
            if (!r.success) { trace("load erro " + r.error); log::warn("Collab ao vivo: load falhou: {}", r.error); status("Collab: " + r.error); return; }
            s_canSave[m_room] = jsonutil::boolOr(r.value, "can_save", false);
            auto seq = jsonutil::integerOr(r.value, "seq");
            auto objects = r.value["objects"].asArray();
            if (seq == 0 && (!objects || objects.unwrap().empty())) { seed(); return; }
            log::info("Collab ao vivo: sala {} seq {} com {} itens (salvar: {})", m_room, seq, objects ? objects.unwrap().size() : 0, canSave());
            trace(fmt::format("load seq {} itens {} salvar {}", seq, objects ? objects.unwrap().size() : 0, canSave()));
            if (objects) rebuild(objects.unwrap());
            trace("load ok");
            m_seq = seq;
            m_ready = true;
            status("Collab ao vivo");
        });
    }

    // First editor in a room without live objects: the room starts from this level.
    void seed() {
        auto ed = editor();
        if (!ed) return;
        auto list = matjson::Value::array();
        int index = 0;
        m_known.clear();
        for (auto object : objectsOf(ed)) {
            auto uid = fmt::format("s{}", index++);
            setUid(object, uid);
            std::string data = object->getSaveString(ed);
            std::erase(data, '\0');
            m_known[uid] = data;
            list.push(matjson::makeObject({{"u", uid}, {"d", data}}));
        }
        if (canSave()) {
            auto settings = settingsOf(ed);
            m_known[SettingsUid] = settings;
            list.push(matjson::makeObject({{"u", SettingsUid}, {"d", settings}}));
        }
        m_busy = true;
        m_busySince = std::time(nullptr);
        m_request.rpc("geobr_collab_live_seed", matjson::makeObject({{"p_room", m_room}, {"p_objects", list}}), [this](auto r) {
            m_busy = false;
            if (!r.success) { log::warn("Collab ao vivo: seed falhou: {}", r.error); status("Collab: " + r.error); return; }
            if (!jsonutil::boolOr(r.value, "ok", false)) { load(); return; }  // someone seeded first
            log::info("Collab ao vivo: sala iniciada com este level");
            m_seq = jsonutil::integerOr(r.value, "seq", 1);
            m_ready = true;
            status("Collab ao vivo");
        });
    }

    // Makes the editor's objects the room's live objects. Objects already
    // identical here only get the room's id; the rest are removed / created.
    void rebuild(std::vector<matjson::Value> const& objects) {
        auto ed = editor();
        if (!ed) return;
        trace("rebuild inicio");
        m_ui->deselectAll();
        m_known.clear();
        m_deferred.clear();
        struct Local { GameObject* object; std::string data; };
        std::unordered_map<std::string, std::vector<Local>> local;
        for (auto object : objectsOf(ed)) {
            std::string data = object->getSaveString(ed);
            std::erase(data, '\0');
            local[matchKey(data)].push_back({object, data});
        }
        std::vector<std::pair<std::string, std::string>> missing;
        size_t matched = 0;
        for (auto const& o : objects) {
            auto uid = jsonutil::stringOr(o, "u");
            auto data = jsonutil::stringOr(o, "d");
            if (uid == SettingsUid) { trace("musica " + data); applySettings(data); continue; }
            if (special(uid) || uid.empty() || data.empty()) continue;
            if (auto it = local.find(matchKey(data)); it != local.end() && !it->second.empty()) {
                auto const& here = it->second.back();
                setUid(here.object, uid);
                m_known[uid] = here.data;  // what this editor has (no change to send)
                it->second.pop_back();
                ++matched;
                continue;
            }
            missing.emplace_back(uid, data);
        }
        std::vector<GameObject*> leftovers;
        for (auto& [key, list] : local) for (auto const& here : list) { trace("apagar local " + here.data); leftovers.push_back(here.object); }
        trace(fmt::format("rebuild iguais {} apagar {} criar {}", matched, leftovers.size(), missing.size()));
        removeSafely(leftovers);
        for (auto const& [uid, data] : missing) place(uid, data);
        clearHistory();
        trace("rebuild fim");
    }

    // Deletes objects the way a player does (select, then Delete), which GD
    // handles for any object, also the ones off screen. LevelEditorLayer::
    // removeObject alone crashed on objects loaded with the level and not on
    // screen (crash reports). The player's selection is kept.
    void removeSafely(std::vector<GameObject*> const& targets) {
        auto ed = editor();
        if (!ed || !m_ui || targets.empty()) return;
        std::vector<Ref<GameObject>> keep;
        if (m_ui->m_selectedObject) keep.emplace_back(m_ui->m_selectedObject);
        if (m_ui->m_selectedObjects)
            for (auto object : CCArrayExt<GameObject*>(m_ui->m_selectedObjects)) if (object) keep.emplace_back(object);
        std::vector<Ref<GameObject>> alive(targets.begin(), targets.end());  // valid until the end of this call
        m_ui->deselectAll();
        auto list = CCArray::create();
        for (auto const& object : alive) list->addObject(object);
        trace(fmt::format("apagar {}", list->count()));
        m_ui->selectObjects(list, true);
        m_ui->onDeleteSelected(nullptr);
        m_ui->deselectAll();
        size_t left = 0;
        for (auto const& object : alive) if (ed->m_objects && ed->m_objects->containsObject(object)) ++left;
        if (left) trace(fmt::format("apagar: {} ficaram (camada bloqueada?)", left));
        auto again = CCArray::create();
        for (auto const& object : keep) if (object && ed->m_objects && ed->m_objects->containsObject(object)) again->addObject(object);
        if (again->count()) m_ui->selectObjects(again, true);
        clearHistory();
        trace("apagar ok");
    }

    GameObject* place(std::string const& uid, std::string const& data) {
        auto ed = editor();
        if (!ed || uid.empty() || data.empty()) return nullptr;
        trace("place " + uid + " " + data);
        auto created = ed->createObjectsFromString(gd::string(data + ";"), true, true);
        if (!created || created->count() == 0) { trace("place vazio"); return nullptr; }
        auto object = static_cast<GameObject*>(created->objectAtIndex(0));
        setUid(object, uid);
        std::string saved = object->getSaveString(ed);
        std::erase(saved, '\0');
        m_known[uid] = saved;
        return object;
    }

    // -------------------------------------------------------------- live
    std::vector<Op> localChanges() {
        std::vector<Op> ops;
        auto ed = editor();
        if (!ed) return ops;
        std::unordered_set<std::string> seen;
        for (auto object : objectsOf(ed)) {
            auto uid = uidOf(object);
            if (uid.empty()) { uid = newUid(); setUid(object, uid); }
            if (!seen.insert(uid).second) { uid = newUid(); setUid(object, uid); seen.insert(uid); }  // pasted copy of a tagged object
            if (ops.size() >= MaxOpsPerRound) continue;
            std::string data = object->getSaveString(ed);
            std::erase(data, '\0');
            auto it = m_known.find(uid);
            if (it != m_known.end() && it->second == data) continue;
            ops.push_back({uid, data, it != m_known.end() ? std::optional<std::string>(it->second) : std::nullopt});
            m_known[uid] = data;
        }
        for (auto it = m_known.begin(); it != m_known.end();) {
            if (special(it->first) || seen.contains(it->first) || ops.size() >= MaxOpsPerRound) { ++it; continue; }
            ops.push_back({it->first, std::nullopt, it->second});
            it = m_known.erase(it);
        }
        if (canSave()) {
            auto settings = settingsOf(ed);
            auto it = m_known.find(SettingsUid);
            if (!settings.empty() && (it == m_known.end() || it->second != settings)) {
                ops.push_back({SettingsUid, settings, it != m_known.end() ? std::optional<std::string>(it->second) : std::nullopt});
                m_known[SettingsUid] = settings;
            }
        }
        // What this editor changed now wins over a remote change still waiting.
        for (auto const& op : ops) m_deferred.erase(op.uid);
        return ops;
    }

    void tick(float) {
        auto now = std::time(nullptr);
        if (m_busy && now - m_busySince < 20) return;
        if (!m_ready) { if (settled(now) && now - m_lastLoad >= 5) load(); return; }
        auto ed = editor();
        if (!ed) return;
        // Playtest moves objects around: no reading or replacing them until it stops.
        if (playing()) return;
        queueFlush();
        m_pending = localChanges();
        auto ops = matjson::Value::array();
        for (auto const& op : m_pending)
            ops.push(op.data ? matjson::makeObject({{"u", op.uid}, {"d", *op.data}}) : matjson::makeObject({{"u", op.uid}, {"d", nullptr}}));
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto view = ed->m_objectLayer ? ed->m_objectLayer->convertToNodeSpace({win.width / 2.f, win.height / 2.f}) : CCPoint{0, 0};
        m_busy = true;
        m_busySince = now;
        m_request.rpc("geobr_collab_live", matjson::makeObject({{"p_room", m_room}, {"p_since", m_seq}, {"p_ops", ops}, {"p_x", view.x}, {"p_y", view.y}}),
            [this](auto r) {
                m_busy = false;
                if (!r.success) {
                    // Not sent: restore what the room has, so the changes go next round.
                    for (auto const& op : m_pending) {
                        if (op.previous) m_known[op.uid] = *op.previous; else m_known.erase(op.uid);
                    }
                    m_pending.clear();
                    log::warn("Collab ao vivo: rodada falhou: {}", r.error);
                    status("Collab: " + r.error);
                    return;
                }
                trace(fmt::format("live enviados {} seq {} recebidos {}", m_pending.size(), jsonutil::integerOr(r.value, "seq"),
                    r.value["ops"].isArray() ? r.value["ops"].asArray().unwrap().size() : 0));
                m_pending.clear();
                if (r.value.contains("can_save")) s_canSave[m_room] = jsonutil::boolOr(r.value, "can_save", false);
                applyRemote(r.value);
                m_seq = jsonutil::integerOr(r.value, "seq", m_seq);
                showPresence(r.value["presence"]);
            });
    }

    void applyRemote(matjson::Value const& value) {
        auto ops = value["ops"].asArray();
        if (!ops) return;
        for (auto const& op : ops.unwrap()) {
            if (jsonutil::boolOr(op, "mine", false)) continue;
            auto uid = jsonutil::stringOr(op, "u");
            if (uid.empty()) continue;
            auto data = op["d"].asString();
            m_deferred[uid] = data && !data.unwrap().empty() ? std::optional<std::string>(data.unwrap()) : std::nullopt;
        }
        queueFlush();
    }

    // Applies the waiting remote changes that are safe now. An object that is
    // selected here (being moved / edited) or any object while a popup is
    // open keeps its change for later; new objects go in right away.
    void flushDeferred() {
        auto ed = editor();
        if (!ed || !m_ui || m_deferred.empty() || playing()) return;
        std::unordered_map<std::string, GameObject*> byUid;
        for (auto object : objectsOf(ed)) if (auto uid = uidOf(object); !uid.empty()) byUid[uid] = object;
        std::unordered_set<GameObject*> busy;
        if (m_ui->m_selectedObject) busy.insert(m_ui->m_selectedObject);
        if (m_ui->m_selectedObjects)
            for (auto object : CCArrayExt<GameObject*>(m_ui->m_selectedObjects)) if (object) busy.insert(object);
        // Touching / dragging / music preview: existing objects wait too.
        bool popup = popupOpen() || m_ui->m_swipeActive || m_ui->m_isDraggingCamera || m_ui->m_isPlayingMusic;
        size_t waiting = m_deferred.size();
        std::vector<std::pair<std::string, std::optional<std::string>>> ready;
        std::vector<GameObject*> remove;
        for (auto it = m_deferred.begin(); it != m_deferred.end();) {
            if (ready.size() >= MaxAppliedPerFlush) break;  // the rest next round
            auto const& uid = it->first;
            auto const& data = it->second;
            if (special(uid)) {
                if (uid == SettingsUid && data && !popup) { trace("musica " + *data); applySettings(*data); }
                else if (uid == SettingsUid && data) { ++it; continue; }
                it = m_deferred.erase(it);
                continue;
            }
            if (auto found = byUid.find(uid); found != byUid.end()) {
                if (popup || busy.contains(found->second)) { ++it; continue; }
                trace("trocar " + uid);
                remove.push_back(found->second);
            }
            ready.emplace_back(uid, data);
            it = m_deferred.erase(it);
        }
        removeSafely(remove);
        for (auto const& [uid, data] : ready) {
            if (data) place(uid, *data);
            else m_known.erase(uid);
        }
        if (!ready.empty()) log::info("Collab ao vivo: {} de {} mudancas aplicadas, {} esperando", ready.size(), waiting, m_deferred.size());
    }

    void showPresence(matjson::Value const& list) {
        if (!m_cursorLayer) return;
        std::unordered_set<std::string> here;
        if (auto people = list.asArray()) {
            for (auto const& p : people.unwrap()) {
                auto name = jsonutil::stringOr(p, "name", "Jogador");
                here.insert(name);
                auto& cursor = m_cursors[name];
                if (!cursor.node) {
                    cursor.node = CCNode::create();
                    if (auto dot = CCSprite::createWithSpriteFrameName("GJ_select_001.png")) {
                        dot->setScale(.6f);
                        dot->setColor({120, 255, 160});
                        cursor.node->addChild(dot);
                    }
                    auto label = CCLabelBMFont::create(name.c_str(), "goldFont.fnt");
                    label->setScale(.55f);
                    label->setPosition({0, 18});
                    cursor.node->addChild(label);
                    cursor.node->setID("geobr-collab-cursor");
                    m_cursorLayer->addChild(cursor.node);
                }
                cursor.pos = CCPoint(static_cast<float>(p["x"].asDouble().unwrapOr(0.0)), static_cast<float>(p["y"].asDouble().unwrapOr(0.0)));
            }
        }
        for (auto it = m_cursors.begin(); it != m_cursors.end();) {
            if (here.contains(it->first)) { ++it; continue; }
            if (it->second.node) it->second.node->removeFromParent();
            it = m_cursors.erase(it);
        }
        update(0.f);
        status(here.empty() ? std::string("Collab ao vivo (so voce agora)") : fmt::format("Collab ao vivo: {} pessoa(s) editando", here.size() + 1));
    }

    // Keeps each member's tag over the point of the level they look at.
    void update(float) override {
        auto ed = editor();
        if (!ed || !ed->m_objectLayer || !m_cursorLayer) return;
        m_cursorLayer->setVisible(!playing());
        for (auto& [name, cursor] : m_cursors) {
            if (!cursor.node) continue;
            cursor.node->setPosition(m_cursorLayer->convertToNodeSpace(ed->m_objectLayer->convertToWorldSpace(cursor.pos)));
        }
    }
};

} // namespace

void startLive(EditorUI* ui, std::string const& room) {
    if (!ui || room.empty() || ui->getChildByID("geobr-collab-live")) return;
    if (auto session = LiveSession::create(ui, room)) ui->addChild(session, 100);
}

void uploadPendingTrace() {
    static bool busy = false, sent = false;
    if (busy || sent) return;
    std::error_code ec;
    auto path = tracePath();
    if (!std::filesystem::exists(path, ec)) { sent = true; return; }
    auto lines = tailOf(path, 250);
    if (lines.empty() || lines.back().ends_with(" END")) { sent = true; return; }
    std::string body = "== trace ==\n";
    for (auto const& line : lines) body += line + "\n";
    if (auto previous = previousGeodeLog(); !previous.empty()) {
        body += "== geode log " + previous.filename().string() + " ==\n";
        for (auto const& line : tailOf(previous, 150)) body += line + "\n";
    }
    body = redact(body);
    if (body.size() > 60000) body = body.substr(body.size() - 60000);
    busy = true;
    static auto request = new CloudRequest();  // lives until the game closes
    request->rpc("geobr_client_report", matjson::makeObject({
        {"p_kind", "collab-crash"}, {"p_version", Mod::get()->getVersion().toVString()}, {"p_body", body},
    }), [path](auto r) {
        busy = false;
        if (!r.success) { log::warn("Relatorio do collab nao enviado: {}", r.error); return; }
        sent = true;
        std::error_code ec;
        std::filesystem::rename(path, path.parent_path() / "collab-trace-enviado.txt", ec);
        log::info("Relatorio do collab enviado");
    });
}

bool canSaveRoom(std::string const& room) {
    auto it = s_canSave.find(room);
    return it != s_canSave.end() && it->second;
}

} // namespace geobr::collab
