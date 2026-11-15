#include "CollabService.hpp"
#include "CollabMerge.hpp"
#include "../models/JsonModelUtils.hpp"

#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/LocalLevelManager.hpp>
#include <Geode/modify/LevelSettingsObject.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

// The room id rides in the level header, next to GD's own settings, so the
// editor's normal save keeps it (GD drops keys it does not know about).
class $modify(GeoBRCollabSettings, LevelSettingsObject) {
    struct Fields {
        std::string room;
    };

    gd::string getSaveString() {
        auto saved = LevelSettingsObject::getSaveString();
        auto const& room = m_fields->room;
        if (room.empty()) return saved;
        std::string value = saved;
        value += fmt::format(",{},{}", geobr::collab::RoomKey, room);
        return gd::string(value);
    }

    static LevelSettingsObject* objectFromDict(CCDictionary* dict) {
        auto settings = LevelSettingsObject::objectFromDict(dict);
        if (settings && dict) {
            if (auto value = dict->valueForKey(geobr::collab::RoomKey); value && geobr::collab::validRoomID(value->getCString())) {
                static_cast<GeoBRCollabSettings*>(settings)->m_fields->room = value->getCString();
            }
        }
        return settings;
    }
};

namespace geobr::collab {

namespace {
// An empty GD level (default settings) so a brand new room is tagged too.
constexpr char EmptyHeader[] = "kA13,0,kA15,0,kA16,0,kA14,,kA6,0,kA7,0,kA17,0,kA18,0,kS39,0,kA2,0,kA3,0,kA8,0,kA4,0,kA9,0,kA10,0,kA11,0";

std::filesystem::path baseDir() { return Mod::get()->getSaveDir() / "collab"; }

void saveLocalLevels() { LocalLevelManager::sharedState()->save(); }
}

std::string roomOfSettings(LevelSettingsObject* settings) {
    return settings ? static_cast<GeoBRCollabSettings*>(settings)->m_fields->room : std::string();
}

void setRoomOfSettings(LevelSettingsObject* settings, std::string const& room) {
    if (settings && (room.empty() || validRoomID(room))) static_cast<GeoBRCollabSettings*>(settings)->m_fields->room = room;
}

CollabService* CollabService::get() {
    static CollabService* instance = [] { auto s = new CollabService; s->retain(); return s; }();
    return instance;
}

void CollabService::start() {
    if (m_started) return;
    m_started = true;
    CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(CollabService::refreshPublished), this, 600.f, false);
    refreshPublished();
}

void CollabService::refreshPublished(float) {
    m_publishedRequest.rpc("geobr_collab_published", matjson::Value::object(), [this](auto r) {
        if (!r.success || !r.value.isArray()) return;
        std::set<int> ids;
        for (auto const& id : r.value.asArray().unwrap()) if (id.isNumber()) ids.insert(static_cast<int>(id.asInt().unwrapOr(0)));
        m_published = std::move(ids);
    });
}

bool CollabService::hasRooms() const { return Mod::get()->getSavedValue<int>("collab-room-count", 0) > 0; }
void CollabService::setRoomCount(int count) { Mod::get()->setSavedValue("collab-room-count", std::max(0, count)); }

std::string CollabService::decompress(std::string const& data) {
    if (data.empty()) return {};
    // Plain level strings start with the header ("kS38," / "kA...").
    if (data.starts_with("kS") || data.starts_with("kA")) return data;
    return std::string(ZipUtils::decompressString(gd::string(data), false, 0));
}

std::string CollabService::compress(std::string const& level) {
    return std::string(ZipUtils::compressString(gd::string(level), false, 0));
}

std::string CollabService::roomOf(GJGameLevel* level) {
    if (!level) return {};
    std::string raw = level->m_levelString;
    if (raw.empty()) return {};
    auto plain = decompress(raw);
    auto header = plain.substr(0, plain.find(';'));
    auto room = roomFromHeader(header);
    return validRoomID(room) ? room : std::string();
}

GJGameLevel* CollabService::localLevelFor(std::string const& room) {
    auto levels = LocalLevelManager::sharedState()->m_localLevels;
    if (!levels) return nullptr;
    auto hint = Mod::get()->getSavedValue<std::string>("collab-local-" + room);
    // Fast path: the level we created for this room (by name), confirmed by its header.
    for (auto level : CCArrayExt<GJGameLevel*>(levels)) {
        if (level && !hint.empty() && std::string(level->m_levelName) == hint && roomOf(level) == room) return level;
    }
    for (auto level : CCArrayExt<GJGameLevel*>(levels)) {
        if (level && level->m_levelString.size() < 16'000'000 && roomOf(level) == room) return level;
    }
    return nullptr;
}

CollabService::Base CollabService::loadBase(std::string const& room) {
    Base base;
    if (!validRoomID(room)) return base;
    auto text = utils::file::readString(baseDir() / (room + ".base"));
    if (text.isErr()) return base;
    base.level = text.unwrap();
    base.revision = Mod::get()->getSavedValue<int64_t>("collab-rev-" + room, 0);
    base.exists = true;
    return base;
}

void CollabService::saveBase(std::string const& room, std::string const& level, int64_t revision) {
    if (!validRoomID(room)) return;
    std::error_code ec;
    std::filesystem::create_directories(baseDir(), ec);
    if (utils::file::writeString(baseDir() / (room + ".base"), level).isErr()) log::warn("GeoBR collab: could not store the base of {}", room);
    Mod::get()->setSavedValue("collab-rev-" + room, revision);
}

void CollabService::openRoom(matjson::Value room, OpenDone done) {
    auto id = jsonutil::stringOr(room, "id");
    auto title = jsonutil::stringOr(room, "title", "Collab");
    if (!validRoomID(id)) { done(nullptr, "Sala invalida"); return; }
    m_pull.rpc("geobr_collab_pull", matjson::makeObject({{"p_room", id}}), [this, id, title, done](auto r) {
        if (!r.success) { done(nullptr, r.error); return; }
        auto revision = jsonutil::integerOr(r.value, "revision");
        auto theirs = decompress(jsonutil::stringOr(r.value, "level"));
        if (theirs.empty()) theirs = std::string(EmptyHeader) + ";";
        theirs = withRoom(theirs, id);

        auto level = localLevelFor(id);
        bool created = false;
        if (!level) {
            level = GameLevelManager::sharedState()->createNewLevel();
            if (!level) { done(nullptr, "Nao foi possivel criar o level local"); return; }
            created = true;
            std::string name = title.substr(0, 20);
            level->m_levelName = name;
            Mod::get()->setSavedValue("collab-local-" + id, name);
        }
        auto local = created ? std::string() : decompress(std::string(level->m_levelString));
        auto base = loadBase(id);
        std::string content = theirs;
        bool pendingLocal = base.exists && !local.empty() && local != base.level;
        if (pendingLocal) content = withRoom(mergeLevels(base.level, local, theirs).level, id);
        level->m_levelString = gd::string(compress(content));
        saveBase(id, theirs, revision);
        saveLocalLevels();
        if (pendingLocal && content != theirs) {
            // Local edits made offline: send the merge right away.
            sync(id, content, [](bool ok, std::string message, std::string, bool) {
                Notification::create(ok ? "Edicoes locais enviadas para a sala" : message, ok ? NotificationIcon::Success : NotificationIcon::Warning, 2.5f)->show();
            });
        }
        done(level, {});
    });
}

void CollabService::sync(std::string room, std::string mine, SyncDone done, bool mergeOnConflict) {
    if (!validRoomID(room)) { done(false, "Sala invalida", mine, false); return; }
    if (m_syncing) { done(false, "Sincronizacao em andamento", mine, false); return; }
    m_syncing = true;
    mine = withRoom(mine, room);
    auto base = loadBase(room);
    pushAttempt(room, mine, base.exists ? base.level : std::string(), base.exists ? base.revision : 0, 0,
        [this, original = mine, done](bool ok, std::string message, std::string merged, bool) {
            m_syncing = false;
            done(ok, std::move(message), merged, merged != original);
        }, mergeOnConflict);
}

void CollabService::pushAttempt(std::string room, std::string mine, std::string base, int64_t baseRevision, int tries, SyncDone done, bool mergeOnConflict) {
    auto args = matjson::makeObject({{"p_room", room}, {"p_level", compress(mine)}, {"p_base", baseRevision}});
    m_push.rpc("geobr_collab_push", args, [this, room, mine, base, tries, done, mergeOnConflict](auto r) {
        if (!r.success) { done(false, r.error, mine, false); return; }
        if (jsonutil::boolOr(r.value, "ok")) {
            auto revision = jsonutil::integerOr(r.value, "revision");
            saveBase(room, mine, revision);
            done(true, fmt::format("Sincronizado (revisao {})", revision), mine, false);
            return;
        }
        if (!jsonutil::boolOr(r.value, "conflict")) { done(false, "Resposta inesperada do servidor", mine, false); return; }
        if (!mergeOnConflict) { done(false, "Ha mudancas de outros membros: toque em Sincronizar", mine, false); return; }
        if (tries >= 3) { done(false, "Muitas edicoes ao mesmo tempo. Tente de novo.", mine, false); return; }
        auto theirs = withRoom(decompress(jsonutil::stringOr(r.value, "level")), room);
        auto merged = withRoom(mergeLevels(base, mine, theirs).level, room);
        pushAttempt(room, merged, theirs, jsonutil::integerOr(r.value, "revision"), tries + 1, done, mergeOnConflict);
    });
}

void CollabService::publish(std::string const& room, int levelID) {
    if (!validRoomID(room) || levelID <= 0) return;
    m_publish.rpc("geobr_collab_publish", matjson::makeObject({{"p_room", room}, {"p_level_id", levelID}}), [this, levelID](auto r) {
        if (r.success) {
            m_published.insert(levelID);
            Notification::create("Collab publicado: os creditos aparecem no level", NotificationIcon::Success, 3.f)->show();
        }
        else {
            Notification::create(r.error, NotificationIcon::Warning, 3.f)->show();
        }
    });
}

} // namespace geobr::collab
