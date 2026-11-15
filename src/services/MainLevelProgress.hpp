#pragma once

#include <cstdint>

namespace geobr {

struct MainLevelProgress final {
    int normal = 0;
    int practice = 0;

    static constexpr int LocalIDBase = 1'500'000'000;
    static constexpr int LocalIDSpan = 400'000'000;

    static int localLevelID(std::int64_t databaseID);
    static bool isGeoBRLocalID(int levelID);
    static MainLevelProgress load(std::int64_t databaseID);
    static void report(int localLevelID, int percent, bool practice);
};

}
