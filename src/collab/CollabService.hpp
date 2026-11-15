#pragma once
#include "../services/CloudService.hpp"
#include <Geode/Geode.hpp>
#include <functional>
#include <set>
#include <string>

class GJGameLevel;
class LevelSettingsObject;

// GeoBR collab editor: rooms on the server, one local level per room.
// The local level carries the room id in its header (key kGBC) so GD's own
// saving keeps it; the last synced revision ("base") is stored per room in
// the mod save folder and is what three-way merges start from.
namespace geobr::collab {

std::string roomOfSettings(LevelSettingsObject* settings);
void setRoomOfSettings(LevelSettingsObject* settings, std::string const& room);

class CollabService final : public cocos2d::CCObject {
public:
    static CollabService* get();
    void start();

    // Levels posted on GD's servers from a collab room (for the "+" credits).
    bool isPublished(int levelID) const { return m_published.contains(levelID); }
    void refreshPublished(float = 0);
    bool hasRooms() const;
    void setRoomCount(int count);

    static std::string roomOf(GJGameLevel* level);
    static GJGameLevel* localLevelFor(std::string const& room);
    static std::string decompress(std::string const& data);
    static std::string compress(std::string const& level);

    struct Base { std::string level; int64_t revision = 0; bool exists = false; };
    static Base loadBase(std::string const& room);
    static void saveBase(std::string const& room, std::string const& level, int64_t revision);

    // Pulls the room, merges unsynced local edits, writes the local level.
    using OpenDone = std::function<void(GJGameLevel*, std::string)>;
    void openRoom(matjson::Value room, OpenDone done);
    // Pushes `mine`; on a conflict merges with the newer revision and retries.
    // `changed` = the merged level differs from `mine` (others edited too).
    using SyncDone = std::function<void(bool ok, std::string message, std::string merged, bool changed)>;
    void sync(std::string room, std::string mine, SyncDone done, bool mergeOnConflict = true);
    bool syncing() const { return m_syncing; }
    void publish(std::string const& room, int levelID);
    // Saves the editor's level as the room's level (host and ADMs only).
    void saveSnapshot(std::string const& room, std::string const& level, std::function<void(bool, std::string)> done);

private:
    void pushAttempt(std::string room, std::string mine, std::string base, int64_t baseRevision, int tries, SyncDone done, bool mergeOnConflict);
    std::set<int> m_published;
    bool m_started = false, m_syncing = false;
    CloudRequest m_publishedRequest, m_pull, m_push, m_publish;
};

} // namespace geobr::collab
