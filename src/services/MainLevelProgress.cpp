#include "MainLevelProgress.hpp"

#include <Geode/Geode.hpp>
#include <algorithm>
#include <fmt/format.h>

using namespace geode::prelude;

namespace geobr {

int MainLevelProgress::localLevelID(std::int64_t databaseID) {
    auto positive = databaseID < 0 ? -databaseID : databaseID;
    return LocalIDBase + static_cast<int>(positive % LocalIDSpan);
}

bool MainLevelProgress::isGeoBRLocalID(int levelID) {
    return levelID >= LocalIDBase && levelID < LocalIDBase + LocalIDSpan;
}

MainLevelProgress MainLevelProgress::load(std::int64_t databaseID) {
    auto id = localLevelID(databaseID);
    return {
        std::clamp(Mod::get()->getSavedValue<int>(fmt::format("main-{}-normal", id), 0), 0, 100),
        std::clamp(Mod::get()->getSavedValue<int>(fmt::format("main-{}-practice", id), 0), 0, 100)
    };
}

void MainLevelProgress::report(int levelID, int percent, bool practiceMode) {
    if (!isGeoBRLocalID(levelID)) return;
    auto key = fmt::format("main-{}-{}", levelID, practiceMode ? "practice" : "normal");
    auto previous = Mod::get()->getSavedValue<int>(key, 0);
    if (percent > previous) Mod::get()->setSavedValue(key, std::clamp(percent, 0, 100));
}

}
