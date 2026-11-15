#pragma once

#include "ArrowPhysics.hpp"

class PlayerObject;
class GJBaseGameLayer;
class LevelSettingsObject;

namespace geobr::arrow {

bool isActive(PlayerObject* player);
// `initial` empty: start going up only if the jump button is held right now.
void activate(PlayerObject* player, bool effects, Direction const* initial = nullptr);
void deactivate(PlayerObject* player);
Snapshot snapshot(PlayerObject* player);
void restore(PlayerObject* player, Snapshot state);

// Level settings flag (Arrow as starting gamemode). Stored as a custom key
// on the settings header; vanilla GD falls back to the wave start mode.
bool startsWithArrow(LevelSettingsObject* settings);
void setStartsWithArrow(LevelSettingsObject* settings, bool enabled);
void applyStart(GJBaseGameLayer* layer, LevelSettingsObject* settings);

} // namespace geobr::arrow
