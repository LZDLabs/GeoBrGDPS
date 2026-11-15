// GDPS routing: which GD requests go to the GeoBR server.
#include "../src/gdps/GdpsRouting.hpp"
#include <cassert>
#include <cstdio>
#include <string>

using namespace geobr::gdps;

static int checks = 0;
#define CHECK(cond) do { assert(cond); ++checks; } while (0)

int main() {
    std::string const base = "https://nolfgjsnnleqpmzvmebo.supabase.co/functions/v1/gdps";
    // Levels, accounts and everything else move to the GDPS when it is on.
    CHECK(rewrite("https://www.boomlings.com/database/getGJLevels21.php", "type=4", true, base) == base + "/getGJLevels21.php");
    CHECK(rewrite("http://www.boomlings.com/database/accounts/loginGJAccount.php", "userName=a", true, base) == base + "/accounts/loginGJAccount.php");
    CHECK(rewrite("https://boomlings.com/database/uploadGJLevel21.php", "", true, base) == base + "/uploadGJLevel21.php");
    // ...and stay on RobTop's server when it is off.
    CHECK(!rewrite("https://www.boomlings.com/database/getGJLevels21.php", "type=4", false, base));
    // Chests and quests come from the GDPS; vault codes, artists and the CDN stay on RobTop.
    CHECK(rewrite("https://www.boomlings.com/database/getGJRewards.php", "", true, base) == base + "/getGJRewards.php");
    CHECK(rewrite("https://www.boomlings.com/database/getGJChallenges.php", "", true, base) == base + "/getGJChallenges.php");
    CHECK(!rewrite("https://www.boomlings.com/database/getGJRewards.php", "", false, base));
    CHECK(!rewrite("https://www.boomlings.com/database/getGJSecretReward.php", "", true, base));
    CHECK(!rewrite("https://www.boomlings.com/database/getGJTopArtists.php", "", true, base));
    CHECK(!rewrite("https://www.boomlings.com/database/getCustomContentURL.php", "", true, base));
    // Newgrounds songs keep working; GeoBR songs are always answered by GeoBR.
    CHECK(!rewrite("https://www.boomlings.com/database/getGJSongInfo.php", "songID=467339&secret=Wmfd2893gb7", true, base));
    CHECK(rewrite("https://www.boomlings.com/database/getGJSongInfo.php", "songID=8000001&secret=Wmfd2893gb7", true, base) == base + "/getGJSongInfo.php");
    CHECK(rewrite("https://www.boomlings.com/database/getGJSongInfo.php", "secret=x&songID=9999999", false, base));
    CHECK(!rewrite("https://www.boomlings.com/database/getGJSongInfo.php", "songID=10000001", true, base));
    CHECK(!rewrite("https://www.boomlings.com/database/getGJSongInfo.php", "songID=8000001x", true, base));
    // Other hosts (Newgrounds audio, GD's CDN, other mods' APIs) are never touched.
    CHECK(!rewrite("https://audio.ngfiles.com/467000/467339_At-the-Speed-of-Light.mp3", "", true, base));
    CHECK(!rewrite("https://geometrydashfiles.b-cdn.net/music/10000001.ogg", "", true, base));
    CHECK(!rewrite("https://api.geode-sdk.org/v1/mods", "", true, base));
    CHECK(!rewrite("https://evil.example/https://www.boomlings.com/database/getGJLevels21.php", "", true, base));
    CHECK(!rewrite("https://www.boomlings.com/database/", "", true, base));
    // Helpers.
    CHECK(formValue("a=1&songID=42&b=2", "songID") == "42");
    CHECK(formValue("songIDs=1&songID=7", "songID") == "7");
    CHECK(formInt("x=1", "songID") == 0);
    CHECK(isGeoBRSong(8000001) && isGeoBRSong(9999999) && !isGeoBRSong(8000000) && !isGeoBRSong(10000000));
    std::printf("PASS gdps_routing: %d checks\n", checks);
    return 0;
}
