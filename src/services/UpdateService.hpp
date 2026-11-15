#pragma once
// Mod updates from the GeoBR release repository (LZDLabs/GeoBrGDPS, folder
// updates/). Once per game session the main menu reads updates/latest.json;
// when it lists a newer GeoBR (or GeoBR GDPS) for this platform, the player is
// asked, the .geode is downloaded, its SHA-256 is checked and it replaces the
// installed package. The new version loads after the game restarts.
namespace geobr::update {
void checkOnce();
}
