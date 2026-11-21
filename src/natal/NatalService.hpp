#pragma once
// GeoBR Natal 2026 on the client: event state (Candy, roulette, calendar, pass,
// accessories), the accessory catalog, the event levels, the current Natal
// battle and BattleGuard. The server is supabase/migrations/20261120120000_geobr_natal_2026.sql;
// every rule (prices, rewards, limits, bans) is checked there.
#include "../models/MainLevel.hpp"

#include <Geode/Geode.hpp>

#include <functional>
#include <string>
#include <vector>

namespace geobr::natal {

using Done = std::function<void(bool ok, std::string const& error)>;

/** MainLevel ids of event levels (their local GD ids stay apart from the Main Levels). */
inline constexpr std::int64_t LevelIdOffset = 300'000'000;

matjson::Value const& state();
matjson::Value const& accessories();
matjson::Value const* accessory(std::string const& id);
matjson::Value const& levels();
/** Christmas profile themes and frames (kind "theme" / "frame"). */
matjson::Value const& cosmetics();
matjson::Value const* cosmetic(std::string const& id);
bool ownsCosmetic(std::string const& id);
matjson::Value const* level(std::int64_t id);
std::string equipped();
bool active();
bool isAdmin();

/** Reloads state, catalog and levels (each part keeps its last good value offline). */
void refresh(Done done = {});
/** Keeps the state the server just returned. */
void setState(matjson::Value const& next);

/** GDPS + "Decoracao de Natal" setting + event switched on (last known). */
bool decorationsOn();
/** Adds the optional texture folder (geode/config/geobr.geobrmod/texturas-natal) in GDPS. */
void applyTexturePack();

/** Event level row -> playable MainLevel; local GD level id -> event level id (0 if not one). */
MainLevel mainLevelOf(matjson::Value const& row);
std::int64_t eventLevelFromLocal(int localLevelID);

/** An accessory's sprite sized for an icon of `unit` points and where it sits from the icon's centre. */
struct AccessoryArt { cocos2d::CCSprite* sprite = nullptr; cocos2d::CCPoint offset; bool behind = false; };
AccessoryArt makeAccessory(std::string const& id, float unit = 30.f);

/** Draws an accessory on an icon node (SimplePlayer / PlayerObject); empty id removes it. */
void attach(cocos2d::CCNode* icon, std::string const& id, float unit = 30.f);

// Batalha Natalina (the arena the server returns) ----------------------------------------------------------------
matjson::Value const& battle();
void setBattle(matjson::Value const& next);
std::string battleId();
/** Seconds the server clock is ahead of this device (from the last battle answer). */
double serverOffset();
double secondsUntil(std::string const& iso);

/** Opens the battle's level (downloads it first when needed); spectate = watch the others' screens. */
void openBattleLevel(bool spectate = false);
/** True once after openBattleLevel(true): the level that opens starts in spectate mode. */
bool takeSpectateRequest();
/** Background watch of the Batalha Natalina (GDPS, event open): notifications 1 h,
 *  5 min and 1 min before it starts, and for players who entered, the level opens
 *  by itself when the battle starts. Started once from the main menu. */
void startBattleWatch();

/** BattleGuard: ids of the mods enabled right now. */
std::vector<std::string> enabledMods();

std::string rarityName(std::string const& rarity);
cocos2d::ccColor3B rarityColor(std::string const& rarity);
std::string rewardText(matjson::Value const& reward);

} // namespace geobr::natal
