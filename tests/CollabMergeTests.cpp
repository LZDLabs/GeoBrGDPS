// Host tests for the collab three-way merge and room header helpers.
#include "../src/collab/CollabMerge.hpp"
#include <cassert>
#include <iostream>

using namespace geobr::collab;

static int checks = 0;
#define CHECK(x) do { assert(x); ++checks; } while (0)

int main() {
    std::string base = "kS38,1,kA2,0;1,1,2,15,3,15;1,8,2,45,3,15;";
    // Me: add a block, delete the spike. Them: add an orb.
    std::string mine = "kS38,1,kA2,0;1,1,2,15,3,15;1,1,2,75,3,15;";
    std::string theirs = "kS38,1,kA2,0;1,1,2,15,3,15;1,8,2,45,3,15;1,36,2,105,3,45;";
    auto r = mergeLevels(base, mine, theirs);
    CHECK(r.level == "kS38,1,kA2,0;1,1,2,15,3,15;1,36,2,105,3,45;1,1,2,75,3,15;");
    CHECK(r.addedByMe == 1 && r.removedByMe == 1 && !r.headerFromMe);

    // Both add the same object: two copies are intentional (different people).
    auto same = mergeLevels("h;", "h;1,1,2,15,3,15;", "h;1,1,2,15,3,15;");
    CHECK(same.level == "h;1,1,2,15,3,15;1,1,2,15,3,15;");

    // Duplicates: base has 2 copies, I delete one, they keep both -> 1 copy.
    auto dup = mergeLevels("h;a;a;", "h;a;", "h;a;a;b;");
    CHECK(dup.level == "h;a;b;");

    // They already deleted what I deleted: nothing to remove twice.
    auto gone = mergeLevels("h;a;b;", "h;b;", "h;b;");
    CHECK(gone.level == "h;b;" && gone.removedByMe == 0);

    // Header: mine wins only if I changed it.
    auto h1 = mergeLevels("H1;a;", "H2;a;", "H3;a;");
    CHECK(h1.level == "H2;a;" && h1.headerFromMe);
    auto h2 = mergeLevels("H1;a;", "H1;a;b;", "H3;a;");
    CHECK(h2.level == "H3;a;b;" && !h2.headerFromMe);

    // No edits on my side: result is theirs.
    CHECK(mergeLevels("h;a;", "h;a;", "h;a;c;").level == "h;a;c;");
    // First sync with an empty base: union.
    CHECK(mergeLevels("", "h;a;", "h;b;").level == "h;b;a;");

    // Splitting tolerates a missing trailing ';' and empty objects.
    auto split = splitLevel("h;a;;b");
    CHECK(split.header == "h" && split.objects.size() == 2 && split.objects[1] == "b");
    CHECK(joinLevel(split) == "h;a;b;");

    // Room key in the header.
    std::string room = "0f3c2a4e-1b2c-4d5e-8f90-a1b2c3d4e5f6";
    auto tagged = withRoom("kS38,1|2,kA2,0;1,1,2,15,3,15;", room);
    CHECK(tagged == "kS38,1|2,kA2,0,kGBC," + room + ";1,1,2,15,3,15;");
    CHECK(roomFromHeader(splitLevel(tagged).header) == room);
    auto retagged = withRoom(tagged, "11111111-2222-3333-4444-555555555555");
    CHECK(roomFromHeader(splitLevel(retagged).header) == "11111111-2222-3333-4444-555555555555");
    CHECK(retagged.find(room) == std::string::npos);
    CHECK(roomFromHeader(splitLevel(withRoom(tagged, "")).header).empty());
    CHECK(roomFromHeader("kA2,0,kA3,1").empty());
    CHECK(validRoomID(room) && !validRoomID("x") && !validRoomID("0F3C2A4E-1B2C-4D5E-8F90-A1B2C3D4E5F6"));
    // The room key never leaks into objects.
    CHECK(splitLevel(tagged).objects.size() == 1);

    std::cout << "CollabMerge: " << checks << " verificacoes aprovadas\n";
}
