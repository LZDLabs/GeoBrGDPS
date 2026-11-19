#pragma once
// Online status: while the game runs in the GDPS the mod tells the server
// every 2 minutes that this player is around (with who may see it), and the
// profile shows a dot: green online, grey idle, red away for a long time.
#include <string>

class ProfilePage;
class GJUserScore;

namespace geobr::presence {

/** Starts the 2-minute ping (idempotent). */
void start();

/** "all" | "friends" | "none" from the mod setting. */
std::string visibility();

/** Adds / refreshes the status dot next to the profile's name. */
void decorate(ProfilePage* page, GJUserScore* score);

} // namespace geobr::presence
