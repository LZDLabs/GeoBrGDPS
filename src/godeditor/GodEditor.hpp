#pragma once

class PlayLayer;
class GJBaseGameLayer;
class PlayerObject;

// GeoBR God Editor: creative mode opened from the pause menu while playing
// one of your own levels. The player flies freely (platformer-style D-pad,
// any gamemode), the camera zooms, any editor object can be placed, colour
// channels can be changed live, and the result is saved into the level.
namespace geobr::god {

bool canOpen(PlayLayer* layer);
void start(PlayLayer* layer);
bool isActive(GJBaseGameLayer* layer);
bool isActive(PlayerObject* player);

} // namespace geobr::god
