#include "CollabLive.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/CloudService.hpp"
#include "CollabService.hpp"
#include "../ui/UIHelpers.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/LevelSettingsLayer.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/TeleportPortalObject.hpp>

#include <Geode/loader/Dirs.hpp>
#include <Geode/ui/Notification.hpp>

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
std::unordered_map<std::string, int> s_rank;  // 3 host, 2 ADM, 1 editor, 0 out / unknown

std::string uidOf(GameObject* object) {
    if (auto value = typeinfo_cast<CCString*>(object->getUserObject(UidKey))) return value->getCString();
    return {};
}

void setUid(GameObject* object, std::string const& uid) { object->setUserObject(UidKey, CCString::create(uid)); }

// The yellow teleport portal (object 749) is the exit GD creates by itself
// with every blue teleport portal (747). It is part of its blue portal and is
// never a live object of its own: sent on its own, every editor placed one
// more and the copies multiplied without end.
constexpr char const* ExtraKey = "geobr-collab-extra";  // made by GD along with another object
bool isYellowData(std::string const& data) { return data == "1,749" || data.starts_with("1,749,"); }
bool isYellow(GameObject* object) {
    if (!object) return false;
    if (object->m_objectID == 749) return true;
    auto portal = typeinfo_cast<TeleportPortalObject*>(object);
    return portal && portal->m_isYellowPortal;
}
bool isExtra(GameObject* object) { return isYellow(object) || object->getUserObject(ExtraKey); }
void markExtra(GameObject* object) { object->setUserObject(ExtraKey, CCString::create("1")); }

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
    // Music plus the level settings (start game mode, speed, mini, dual,
    // mirror, 2 players, platformer, background / ground / font).
    return matjson::makeObject({
        {"a", level->m_audioTrack}, {"s", level->m_songID}, {"o", settings->m_songOffset},
        {"fi", settings->m_fadeIn}, {"fo", settings->m_fadeOut},
        {"gm", settings->m_startMode}, {"sp", static_cast<int>(settings->m_startSpeed)},
        {"mi", settings->m_startMini}, {"du", settings->m_startDual}, {"mr", settings->m_mirrorMode},
        {"rg", settings->m_rotateGameplay}, {"tp", settings->m_twoPlayerMode}, {"pf", settings->m_platformerMode},
        {"bg", settings->m_backgroundIndex}, {"gr", settings->m_groundIndex}, {"ft", settings->m_fontIndex},
        {"mg", settings->m_middleGroundIndex}, {"fl", settings->m_isFlipped}, {"rv", settings->m_reverseGameplay},
    }).dump(matjson::NO_INDENTATION);
}

// The level settings window is open here (its own copy of the values).
bool settingsOpen() {
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return false;
    for (auto child : CCArrayExt<CCNode*>(scene->getChildren())) if (typeinfo_cast<LevelSettingsLayer*>(child)) return true;
    return false;
}

std::vector<GameObject*> objectsOf(LevelEditorLayer* editor) {
    std::vector<GameObject*> out;
    if (editor && editor->m_objects)
        for (auto object : CCArrayExt<GameObject*>(editor->m_objects)) if (object) out.push_back(object);
    return out;
}

// The objects the room keeps (yellow portals and other objects GD made along
// with another one come with it, so they are left out).
std::vector<GameObject*> syncedObjectsOf(LevelEditorLayer* editor) {
    std::vector<GameObject*> out;
    for (auto object : objectsOf(editor)) if (!isExtra(object)) out.push_back(object);
    return out;
}

// Yellow portals that no blue portal owns: the copies left by the old
// collab loop. GD does not let the player delete them.
std::vector<GameObject*> looseYellowPortals(LevelEditorLayer* editor) {
    std::unordered_set<GameObject*> owned;
    std::vector<GameObject*> yellows;
    for (auto object : objectsOf(editor)) {
        if (isYellow(object)) { yellows.push_back(object); continue; }
        if (auto portal = typeinfo_cast<TeleportPortalObject*>(object); portal && portal->m_orangePortal) owned.insert(portal->m_orangePortal);
    }
    std::erase_if(yellows, [&](GameObject* object) { return owned.contains(object); });
    return yellows;
}

// Removes them directly (the editor's Delete skips yellow portals). Each one
// is held until the end and goes through the undo list, then the history is
// cleared: removal without undo freed objects other mods still used.
size_t removeLoose(LevelEditorLayer* editor) {
    if (!editor || !editor->m_objects) return 0;
    auto loose = looseYellowPortals(editor);
    if (loose.empty()) return 0;
    if (editor->m_editorUI) editor->m_editorUI->deselectAll();
    std::vector<Ref<GameObject>> keep(loose.begin(), loose.end());
    size_t removed = 0;
    for (auto const& object : keep) {
        if (!editor->m_objects->containsObject(object)) continue;
        if (auto portal = typeinfo_cast<TeleportPortalObject*>(object.data())) portal->m_orangePortal = nullptr;
        editor->removeObject(object, false);
        ++removed;
    }
    if (editor->m_undoObjects) editor->m_undoObjects->removeAllObjects();
    if (editor->m_redoObjects) editor->m_redoObjects->removeAllObjects();
    return removed;
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
    std::string m_roomSettings;  // the room's level settings as last received
    bool m_flushQueued = false;
    static constexpr size_t MaxAppliedPerFlush = 400;
    std::vector<Op> m_pending;
    CloudRequest m_request, m_clean;
    CCLabelBMFont* m_status = nullptr;
    // 1.17: room generation (a change reloads the room), role, chat.
    int64_t m_gen = -1;
    bool m_removed = false, m_viewOnly = false;
    std::string m_chatAt;       // newest chat message seen (server time)
    int m_unread = 0;
    CCNode* m_toastLayer = nullptr;
    std::vector<CCNode*> m_toasts;

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
        m_toastLayer = CCNode::create();
        m_toastLayer->setID("geobr-collab-chat-toasts");
        addChild(m_toastLayer, 3);
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
        auto settings = ed->m_levelSettings;
        log::info("Collab ao vivo: configuracoes da sala {}", data);
        m_roomSettings = data;
        auto before = settingsOf(ed);
        // The song lives in the level; the settings window edits the level its
        // settings object points to, which can be another object: set both.
        for (auto level : {ed->m_level, settings->m_level}) {
            if (!level) continue;
            level->m_audioTrack = static_cast<int>(jsonutil::integerOr(v, "a", level->m_audioTrack));
            level->m_songID = static_cast<int>(jsonutil::integerOr(v, "s", level->m_songID));
        }
        auto num = [&](char const* key, int fallback) { return static_cast<int>(jsonutil::integerOr(v, key, fallback)); };
        auto flag = [&](char const* key, bool fallback) { return jsonutil::boolOr(v, key, fallback); };
        settings->m_songOffset = static_cast<float>(v["o"].asDouble().unwrapOr(settings->m_songOffset));
        settings->m_fadeIn = flag("fi", settings->m_fadeIn);
        settings->m_fadeOut = flag("fo", settings->m_fadeOut);
        settings->m_startMode = std::clamp(num("gm", settings->m_startMode), 0, 7);
        settings->m_startSpeed = static_cast<Speed>(std::clamp(num("sp", static_cast<int>(settings->m_startSpeed)), 0, 4));
        settings->m_startMini = flag("mi", settings->m_startMini);
        settings->m_startDual = flag("du", settings->m_startDual);
        settings->m_mirrorMode = flag("mr", settings->m_mirrorMode);
        settings->m_rotateGameplay = flag("rg", settings->m_rotateGameplay);
        settings->m_twoPlayerMode = flag("tp", settings->m_twoPlayerMode);
        settings->m_platformerMode = flag("pf", settings->m_platformerMode);
        settings->m_backgroundIndex = std::max(0, num("bg", settings->m_backgroundIndex));
        settings->m_groundIndex = std::max(0, num("gr", settings->m_groundIndex));
        settings->m_fontIndex = std::max(0, num("ft", settings->m_fontIndex));
        settings->m_middleGroundIndex = std::max(0, num("mg", settings->m_middleGroundIndex));
        settings->m_isFlipped = flag("fl", settings->m_isFlipped);
        settings->m_reverseGameplay = flag("rv", settings->m_reverseGameplay);
        auto after = settingsOf(ed);
        m_known[SettingsUid] = after;
        // Like closing the settings window: the editor redraws background,
        // ground, font and mode. At the start of a frame, never in playtest.
        if (after != before) {
            WeakRef<LiveSession> weak = this;
            Loader::get()->queueInMainThread([weak] {
                auto self = weak.lock();
                if (!self) return;
                auto ed = self->editor();
                if (ed && !self->playing() && !settingsOpen()) { trace("configuracoes aplicadas"); ed->levelSettingsUpdated(); }
            });
        }
    }

    // Someone who cannot change the room's settings changed them here: back to the room's.
    void keepRoomSettings() {
        auto ed = editor();
        if (!ed || canSave() || m_roomSettings.empty() || settingsOpen()) return;
        auto it = m_known.find(SettingsUid);
        if (it != m_known.end() && settingsOf(ed) != it->second) applySettings(m_roomSettings);
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
        for (auto object : syncedObjectsOf(ed)) {
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
        for (auto object : syncedObjectsOf(ed)) {
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
            if (special(uid) || uid.empty() || data.empty() || isYellowData(data)) continue;
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
        if (isYellowData(data)) { trace("place amarelo ignorado " + uid); m_known.erase(uid); return nullptr; }
        trace("place " + uid + " " + data);
        auto created = ed->createObjectsFromString(gd::string(data + ";"), true, true);
        if (!created || created->count() == 0) { trace("place vazio"); return nullptr; }
        // GD can make more than one object from one string (a blue teleport
        // portal brings its yellow exit): only the main one is the room's.
        GameObject* object = nullptr;
        for (auto made : CCArrayExt<GameObject*>(created)) {
            if (!made) continue;
            if (!object && !isYellow(made)) { object = made; continue; }
            if (made != object) markExtra(made);
        }
        if (!object) { trace("place sem objeto principal"); return nullptr; }
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
        for (auto object : syncedObjectsOf(ed)) {
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
        if (m_removed) return;
        auto now = std::time(nullptr);
        if (m_unread && chatOpen()) { m_unread = 0; updateBadge(); }
        if (m_busy && now - m_busySince < 20) return;
        if (!m_ready) { if (settled(now) && now - m_lastLoad >= 5) load(); return; }
        auto ed = editor();
        if (!ed) return;
        // Playtest moves objects around: no reading or replacing them until it stops.
        if (playing()) return;
        keepRoomSettings();
        queueFlush();
        m_pending = localChanges();
        // View only (the host blocked this member): local edits go back.
        if (m_viewOnly && !m_pending.empty()) { undoLocal(m_pending); m_pending.clear(); }
        auto ops = matjson::Value::array();
        for (auto const& op : m_pending)
            ops.push(op.data ? matjson::makeObject({{"u", op.uid}, {"d", *op.data}}) : matjson::makeObject({{"u", op.uid}, {"d", nullptr}}));
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto view = ed->m_objectLayer ? ed->m_objectLayer->convertToNodeSpace({win.width / 2.f, win.height / 2.f}) : CCPoint{0, 0};
        m_busy = true;
        m_busySince = now;
        auto chatSince = m_chatAt.empty() ? matjson::Value(nullptr) : matjson::Value(m_chatAt);
        m_request.rpc("geobr_collab_live2", matjson::makeObject({{"p_room", m_room}, {"p_since", m_seq}, {"p_ops", ops}, {"p_x", view.x}, {"p_y", view.y},
            {"p_chat", chatSince}}),
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
                if (jsonutil::boolOr(r.value, "removed", false)) { removedFromRoom(jsonutil::stringOr(r.value, "reason")); return; }
                if (r.value.contains("can_save")) s_canSave[m_room] = jsonutil::boolOr(r.value, "can_save", false);
                updateRole(r.value);
                showChat(r.value);
                // The host / an ADM reloaded, restored or cleaned the room: start over from it.
                auto gen = jsonutil::integerOr(r.value, "gen", -1);
                if (gen >= 0 && m_gen >= 0 && gen != m_gen) { m_gen = gen; reload("sala recarregada pelo host"); return; }
                if (gen >= 0) m_gen = gen;
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
        for (auto object : syncedObjectsOf(ed)) if (auto uid = uidOf(object); !uid.empty()) byUid[uid] = object;
        std::unordered_set<GameObject*> busy;
        if (m_ui->m_selectedObject) busy.insert(m_ui->m_selectedObject);
        if (m_ui->m_selectedObjects)
            for (auto object : CCArrayExt<GameObject*>(m_ui->m_selectedObjects)) if (object) busy.insert(object);
        // Touching / dragging / music preview: existing objects wait too.
        // Selected objects (moved / edited / in an edit window) wait; so do all
        // existing objects while the player drags a selection box or the camera.
        bool popup = m_ui->m_swipeActive || m_ui->m_isDraggingCamera;
        bool settingsBusy = settingsOpen();
        size_t waiting = m_deferred.size();
        std::vector<std::pair<std::string, std::optional<std::string>>> ready;
        std::vector<GameObject*> remove;
        for (auto it = m_deferred.begin(); it != m_deferred.end();) {
            if (ready.size() >= MaxAppliedPerFlush) break;  // the rest next round
            auto const& uid = it->first;
            auto const& data = it->second;
            if (special(uid)) {
                if (uid == SettingsUid && data && !settingsBusy) { trace("musica " + *data); applySettings(*data); }
                else if (uid == SettingsUid && data) { ++it; continue; }
                it = m_deferred.erase(it);
                continue;
            }
            if (data && isYellowData(*data)) { it = m_deferred.erase(it); continue; }  // comes with its blue portal
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

    // ---------------------------------------------------------- 1.17
public:
    void reload(std::string const& why) {
        if (m_removed) return;
        trace("recarregar " + why);
        m_ready = false;
        m_known.clear();
        m_deferred.clear();
        m_seq = 0;
        status("Collab: " + why);
        load();
    }

    std::string const& room() const { return m_room; }

private:
    void updateRole(matjson::Value const& v) {
        auto role = jsonutil::stringOr(v, "role");
        bool view = role == "viewer";
        if (view != m_viewOnly) {
            m_viewOnly = view;
            status(view ? "Collab: modo so ver (o host bloqueou sua edicao)" : "Collab ao vivo: edicao liberada");
        }
        int rank = jsonutil::boolOr(v, "owner", false) ? 3 : jsonutil::boolOr(v, "can_save", false) ? 2 : 1;
        s_rank[m_room] = rank;
    }

    // Puts back what this editor changed (view only): changed and deleted
    // objects return to the room's version, new ones go away.
    void undoLocal(std::vector<Op> const& ops) {
        for (auto const& op : ops) {
            if (special(op.uid)) continue;  // settings: keepRoomSettings
            if (op.previous) { m_known[op.uid] = *op.previous; m_deferred[op.uid] = *op.previous; }
            else { m_known.erase(op.uid); m_deferred[op.uid] = std::nullopt; }
        }
        queueFlush();
    }

    void removedFromRoom(std::string const& reason) {
        if (m_removed) return;
        m_removed = true;
        m_ready = false;
        s_canSave[m_room] = false;
        s_rank[m_room] = 0;
        m_deferred.clear();
        trace("removido " + reason);
        for (auto& [name, cursor] : m_cursors) if (cursor.node) cursor.node->removeFromParent();
        m_cursors.clear();
        std::string text = reason == "ban" ? "Voce foi <cr>banido</c> desta sala. Suas edicoes daqui nao vao mais para a sala."
            : reason == "kick" ? "Voce foi <cy>expulso</c> da sala pelo host. Para voltar, entre de novo com o codigo."
            : reason == "closed" ? "Esta sala foi fechada."
            : "Voce nao esta mais nesta sala.";
        if (m_status) {
            m_status->setString(reason == "ban" ? "Collab: voce foi banido da sala" : reason == "kick" ? "Collab: voce foi expulso da sala" : "Collab: fora da sala");
            m_status->setColor({255, 120, 120});
        }
        FLAlertLayer::create("Collab", text.c_str(), "OK")->show();
    }

    static bool chatOpen() {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        return scene && scene->getChildByID("geobr-chat");
    }

    // New chat messages: a small line at the top for a few seconds (never
    // over the editing area's middle) and a counter on the Chat button.
    void showChat(matjson::Value const& v) {
        if (auto at = v["chat_at"].asString()) m_chatAt = at.unwrap();
        else if (m_chatAt.empty()) m_chatAt = "1970-01-01T00:00:00Z";  // no message yet: every new one counts
        auto list = v["chat"].asArray();
        if (!list || list.unwrap().empty()) return;
        bool open = chatOpen();
        for (auto const& m : list.unwrap()) {
            if (open) continue;
            ++m_unread;
            bool system = jsonutil::stringOr(m, "kind") == "system";
            toast(system ? std::string("Sala") : jsonutil::stringOr(m, "sender_name", "Jogador"), jsonutil::stringOr(m, "body"), system);
        }
        updateBadge();
    }

    void toast(std::string const& name, std::string body, bool system) {
        if (!m_toastLayer) return;
        if (body.size() > 90) body = body.substr(0, 87) + "...";
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto node = CCNodeRGBA::create();
        node->setCascadeOpacityEnabled(true);
        node->setID("geobr-collab-chat-toast");
        auto who = CCLabelBMFont::create((name + ":").c_str(), "chatFont.fnt");
        who->setScale(.5f);
        who->setColor(system ? ccColor3B{120, 255, 160} : ccColor3B{255, 214, 110});
        who->setAnchorPoint({0, .5f});
        auto text = CCLabelBMFont::create(body.c_str(), "chatFont.fnt");
        text->setScale(.5f);
        text->setAnchorPoint({0, .5f});
        who->limitLabelWidth(90.f, .5f, .3f);
        text->limitLabelWidth(250.f, .5f, .3f);
        float whoWidth = who->getScaledContentSize().width;
        float width = whoWidth + 4.f + text->getScaledContentSize().width + 14.f;
        auto bg = extension::CCScale9Sprite::create("square02_small.png");
        bg->setContentSize({width, 18.f});
        bg->setColor({0, 0, 0});
        bg->setOpacity(130);
        node->setContentSize({width, 18.f});
        node->setAnchorPoint({.5f, .5f});
        bg->setPosition({width / 2.f, 9.f});
        node->addChild(bg);
        who->setPosition({7.f, 9.f});
        node->addChild(who);
        text->setPosition({7.f + whoWidth + 4.f, 9.f});
        node->addChild(text);
        // Newest on top, at most 3 lines; older ones move down.
        for (auto old : m_toasts) old->runAction(CCMoveBy::create(.15f, {0.f, -21.f}));
        if (m_toasts.size() >= 3) { m_toasts.front()->removeFromParent(); m_toasts.erase(m_toasts.begin()); }
        node->setPosition({win.width / 2.f, win.height - 62.f});
        node->setOpacity(0);
        m_toastLayer->addChild(node);
        m_toasts.push_back(node);
        WeakRef<LiveSession> weak = this;
        node->runAction(CCSequence::create(CCFadeIn::create(.2f), CCDelayTime::create(4.5f), CCFadeOut::create(.5f),
            CallFuncExt::create([weak, node] {
                auto self = weak.lock();
                if (self) std::erase(self->m_toasts, node);
                node->removeFromParent();
            }), nullptr));
    }

    void updateBadge() {
        auto menu = m_ui ? m_ui->getChildByID("geobr-collab-editor-menu") : nullptr;
        auto button = menu ? menu->getChildByID("geobr-collab-chat") : nullptr;
        if (!button) return;
        auto badge = button->getChildByID("geobr-collab-chat-badge");
        if (m_unread <= 0) { if (badge) badge->removeFromParent(); return; }
        if (!badge) {
            badge = CCNode::create();
            badge->setID("geobr-collab-chat-badge");
            auto dot = CCDrawNode::create();
            dot->drawDot({0, 0}, 7.f, ccc4f(.9f, .2f, .25f, 1.f));
            badge->addChild(dot);
            auto count = CCLabelBMFont::create("", "bigFont.fnt");
            count->setID("count");
            count->setScale(.3f);
            badge->addChild(count);
            badge->setPosition({button->getContentSize().width - 2.f, button->getContentSize().height - 2.f});
            button->addChild(badge, 10);
        }
        if (auto count = typeinfo_cast<CCLabelBMFont*>(badge->getChildByID("count")))
            count->setString(m_unread > 9 ? "9+" : std::to_string(m_unread).c_str());
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
        status((here.empty() ? std::string("Collab ao vivo (so voce agora)") : fmt::format("Collab ao vivo: {} pessoa(s) editando", here.size() + 1))
            + (m_viewOnly ? "  |  modo so ver" : ""));
    }

    // Keeps each member's tag over the point of the level they look at.
    void update(float) override {
        auto ed = editor();
        if (!ed || !ed->m_objectLayer || !m_cursorLayer) return;
        m_cursorLayer->setVisible(!playing());
        if (m_toastLayer) m_toastLayer->setVisible(!playing());
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

void reloadRoom(EditorUI* ui) {
    if (auto session = ui ? typeinfo_cast<LiveSession*>(ui->getChildByID("geobr-collab-live")) : nullptr) session->reload("sala recarregada");
}

int roomRank(std::string const& room) {
    auto it = s_rank.find(room);
    return it != s_rank.end() ? it->second : 0;
}

size_t countLooseYellowPortals(LevelEditorLayer* editor) { return editor ? looseYellowPortals(editor).size() : 0; }

size_t deleteLooseYellowPortals(LevelEditorLayer* editor) { return removeLoose(editor); }

namespace {
// Any level in the editor: a button appears while there are yellow portals
// without their blue portal (left by the old collab loop) and deletes them.
class YellowWatch final : public CCNode {
    EditorUI* m_ui = nullptr;
    CCMenu* m_ownMenu = nullptr;
    CCMenuItemSpriteExtra* m_button = nullptr;
    size_t m_count = 0;
    CloudRequest m_clean;

public:
    static YellowWatch* create(EditorUI* ui) {
        auto node = new YellowWatch;
        if (node->init(ui)) { node->autorelease(); return node; }
        delete node;
        return nullptr;
    }

    bool init(EditorUI* ui) {
        if (!CCNode::init()) return false;
        m_ui = ui;
        setID("geobr-yellow-portal-watch");
        schedule(schedule_selector(YellowWatch::check), 2.f);
        return true;
    }

    std::string room() const {
        auto ed = m_ui ? m_ui->m_editorLayer : nullptr;
        return ed ? roomOfSettings(ed->m_levelSettings) : std::string();
    }

    CCNode* menu() {
        if (auto collab = m_ui->getChildByID("geobr-collab-editor-menu")) return collab;
        if (!m_ownMenu) {
            auto win = CCDirector::sharedDirector()->getWinSize();
            m_ownMenu = CCMenu::create();
            m_ownMenu->setID("geobr-yellow-portal-menu");
            m_ownMenu->setPosition({win.width / 2.f, win.height - 18.f});
            m_ui->addChild(m_ownMenu, 100);
        }
        return m_ownMenu;
    }

    void check(float) {
        auto ed = m_ui ? m_ui->m_editorLayer : nullptr;
        if (!ed || ed->m_playbackMode != PlaybackMode::Not) return;
        auto count = looseYellowPortals(ed).size();
        if (count == m_count) return;
        m_count = count;
        if (m_button) { auto parent = m_button->getParent(); m_button->removeFromParent(); m_button = nullptr; if (parent) parent->updateLayout(); }
        if (!count) return;
        auto target = menu();
        m_button = ui::makeTextButton(fmt::format("Apagar {} portais amarelos", count), 170, "GJ_button_06.png", this,
            menu_selector(YellowWatch::onDelete), .3f, 26);
        m_button->setID("geobr-yellow-portal-delete");
        if (target == m_ownMenu) m_button->setPosition({0, 0});
        target->addChild(m_button);
        if (target != m_ownMenu) target->updateLayout();
    }

    void onDelete(CCObject*) {
        WeakRef<YellowWatch> weak = this;
        createQuickPopup("Portais amarelos",
            fmt::format("Apagar os <cy>{}</c> portais amarelos soltos?\nSao as copias que o bug da collab criou (sem portal azul). "
                "Os portais de teleporte de verdade continuam.", m_count),
            "Cancelar", "Apagar", [weak](auto, bool yes) {
                auto self = weak.lock();
                if (!yes || !self || !self->m_ui) return;
                auto removed = removeLoose(self->m_ui->m_editorLayer);
                auto room = self->room();
                // The copies saved in the room go too (host and ADMs).
                if (!room.empty() && roomRank(room) >= 2)
                    self->m_clean.rpc("geobr_collab_clean", matjson::makeObject({{"p_room", room}, {"p_what", "yellow"}}), [](auto) {});
                Notification::create(fmt::format("{} portais amarelos apagados", removed), NotificationIcon::Success, 2.f)->show();
                self->m_count = SIZE_MAX;
                self->check(0);
            });
    }
};
} // namespace

void startYellowWatch(EditorUI* ui) {
    if (!ui || ui->getChildByID("geobr-yellow-portal-watch")) return;
    if (auto watch = YellowWatch::create(ui)) ui->addChild(watch);
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
