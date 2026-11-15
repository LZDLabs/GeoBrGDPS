#include "CollabLive.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/CloudService.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>

#include <ctime>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>

using namespace geode::prelude;

namespace geobr::collab {
namespace {

constexpr char const* UidKey = "geobr-collab-uid";
constexpr size_t MaxOpsPerRound = 1500;
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
    std::time_t m_busySince = 0, m_lastLoad = 0;
    std::unordered_map<std::string, std::string> m_known;  // uid -> object string the room has
    std::unordered_map<std::string, CCNode*> m_cursors;     // member name -> tag in the level
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
        auto win = CCDirector::sharedDirector()->getWinSize();
        m_status = CCLabelBMFont::create("Collab ao vivo: conectando...", "chatFont.fnt");
        m_status->setScale(.55f);
        m_status->setColor({120, 255, 160});
        m_status->setPosition({win.width / 2.f, win.height - 40.f});
        m_status->setID("geobr-collab-live-status");
        ui->addChild(m_status, 100);
        schedule(schedule_selector(LiveSession::tick), 1.f);
        load();
        return true;
    }

    LevelEditorLayer* editor() const { return m_ui ? m_ui->m_editorLayer : nullptr; }

    void status(std::string const& text) { if (m_status) m_status->setString(text.c_str()); }

    std::string newUid() { return fmt::format("{}.{}", m_prefix, ++m_counter); }

    // ------------------------------------------------------------- start
    void load() {
        m_busy = true;
        m_busySince = m_lastLoad = std::time(nullptr);
        m_request.rpc("geobr_collab_live_load", matjson::makeObject({{"p_room", m_room}}), [this](auto r) {
            m_busy = false;
            if (!r.success) { status("Collab: " + r.error); return; }
            s_canSave[m_room] = jsonutil::boolOr(r.value, "can_save", false);
            auto seq = jsonutil::integerOr(r.value, "seq");
            auto objects = r.value["objects"].asArray();
            if (seq == 0 && (!objects || objects.unwrap().empty())) { seed(); return; }
            if (objects) rebuild(objects.unwrap());
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
            m_known[uid] = data;
            list.push(matjson::makeObject({{"u", uid}, {"d", data}}));
        }
        m_busy = true;
        m_busySince = std::time(nullptr);
        m_request.rpc("geobr_collab_live_seed", matjson::makeObject({{"p_room", m_room}, {"p_objects", list}}), [this](auto r) {
            m_busy = false;
            if (!r.success) { status("Collab: " + r.error); return; }
            if (!jsonutil::boolOr(r.value, "ok", false)) { load(); return; }  // someone seeded first
            m_seq = jsonutil::integerOr(r.value, "seq", 1);
            m_ready = true;
            status("Collab ao vivo");
        });
    }

    // Replaces the editor's objects by the room's live objects.
    void rebuild(std::vector<matjson::Value> const& objects) {
        auto ed = editor();
        if (!ed) return;
        m_ui->deselectAll();
        for (auto object : objectsOf(ed)) ed->removeObject(object, true);
        m_known.clear();
        for (auto const& o : objects) place(jsonutil::stringOr(o, "u"), jsonutil::stringOr(o, "d"));
    }

    GameObject* place(std::string const& uid, std::string const& data) {
        auto ed = editor();
        if (!ed || uid.empty() || data.empty()) return nullptr;
        auto created = ed->createObjectsFromString(gd::string(data + ";"), true, true);
        if (!created || created->count() == 0) return nullptr;
        auto object = static_cast<GameObject*>(created->objectAtIndex(0));
        setUid(object, uid);
        m_known[uid] = std::string(object->getSaveString(ed));
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
            auto it = m_known.find(uid);
            if (it != m_known.end() && it->second == data) continue;
            ops.push_back({uid, data, it != m_known.end() ? std::optional<std::string>(it->second) : std::nullopt});
            m_known[uid] = data;
        }
        for (auto it = m_known.begin(); it != m_known.end();) {
            if (seen.contains(it->first) || ops.size() >= MaxOpsPerRound) { ++it; continue; }
            ops.push_back({it->first, std::nullopt, it->second});
            it = m_known.erase(it);
        }
        return ops;
    }

    void tick(float) {
        auto now = std::time(nullptr);
        if (m_busy && now - m_busySince < 20) return;
        if (!m_ready) { if (now - m_lastLoad >= 5) load(); return; }
        auto ed = editor();
        if (!ed) return;
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
                    status("Collab: " + r.error);
                    return;
                }
                m_pending.clear();
                applyRemote(r.value);
                m_seq = jsonutil::integerOr(r.value, "seq", m_seq);
                showPresence(r.value["presence"]);
                if (jsonutil::boolOr(r.value, "more", false)) scheduleOnce(schedule_selector(LiveSession::tick), 0.f);
            });
    }

    void applyRemote(matjson::Value const& value) {
        auto ops = value["ops"].asArray();
        if (!ops) return;
        std::vector<matjson::Value> theirs;
        for (auto const& op : ops.unwrap()) if (!jsonutil::boolOr(op, "mine", false)) theirs.push_back(op);
        if (theirs.empty()) return;
        auto ed = editor();
        if (!ed) return;
        std::unordered_map<std::string, GameObject*> byUid;
        for (auto object : objectsOf(ed)) if (auto uid = uidOf(object); !uid.empty()) byUid[uid] = object;
        bool deselected = false;
        for (auto const& op : theirs) {
            auto uid = jsonutil::stringOr(op, "u");
            if (auto it = byUid.find(uid); it != byUid.end()) {
                if (!deselected) { m_ui->deselectAll(); deselected = true; }  // never keep a removed object selected
                ed->removeObject(it->second, true);
                byUid.erase(it);
            }
            auto data = op["d"].asString();
            if (data && !data.unwrap().empty()) { if (auto created = place(uid, data.unwrap())) byUid[uid] = created; }
            else m_known.erase(uid);
        }
    }

    void showPresence(matjson::Value const& list) {
        auto ed = editor();
        if (!ed || !ed->m_objectLayer) return;
        std::unordered_set<std::string> here;
        auto people = list.asArray();
        float scale = std::max(.05f, ed->m_objectLayer->getScale());
        if (people) {
            for (auto const& p : people.unwrap()) {
                auto name = jsonutil::stringOr(p, "name", "Jogador");
                here.insert(name);
                auto& tag = m_cursors[name];
                if (!tag) {
                    tag = CCNode::create();
                    auto dot = CCSprite::createWithSpriteFrameName("GJ_select_001.png");
                    if (dot) { dot->setScale(.6f); dot->setColor({120, 255, 160}); tag->addChild(dot); }
                    auto label = CCLabelBMFont::create(name.c_str(), "goldFont.fnt");
                    label->setScale(.55f);
                    label->setPosition({0, 18});
                    tag->addChild(label);
                    tag->setID("geobr-collab-cursor");
                    ed->m_objectLayer->addChild(tag, 9999);
                }
                tag->setPosition({static_cast<float>(p["x"].asDouble().unwrapOr(0.0)), static_cast<float>(p["y"].asDouble().unwrapOr(0.0))});
                tag->setScale(1.f / scale);  // same size on screen at any zoom
            }
        }
        for (auto it = m_cursors.begin(); it != m_cursors.end();) {
            if (here.contains(it->first)) { ++it; continue; }
            if (it->second) it->second->removeFromParent();
            it = m_cursors.erase(it);
        }
        status(here.empty() ? std::string("Collab ao vivo (so voce agora)") : fmt::format("Collab ao vivo: {} pessoa(s) editando", here.size() + 1));
    }
};

} // namespace

void startLive(EditorUI* ui, std::string const& room) {
    if (!ui || room.empty() || ui->getChildByID("geobr-collab-live")) return;
    if (auto session = LiveSession::create(ui, room)) ui->addChild(session);
}

bool canSaveRoom(std::string const& room) {
    auto it = s_canSave.find(room);
    return it != s_canSave.end() && it->second;
}

} // namespace geobr::collab
