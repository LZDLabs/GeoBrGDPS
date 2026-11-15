#include "GeoBRRobotPack.hpp"

#include "../integrations/MoreIconsAPI.hpp"

#include <Geode/Geode.hpp>

#include <array>
#include <filesystem>
#include <string_view>

using namespace geode::prelude;

namespace geobr {
namespace {

struct RobotAsset {
    std::string_view slug;
    std::string_view displayName;
};

constexpr std::array RobotAssets {
    RobotAsset { "sneaker-bot", "Sneaker Bot" },
    RobotAsset { "blue-runner", "Blue Runner" },
    RobotAsset { "orange-guard", "Orange Guard" },
};

constexpr std::string_view PackID = "geobr.street-robots";
constexpr std::string_view PackName = "GeoBR Street Robots";

} // namespace

void registerGeoBRRobotPack() {
    static bool registered = false;
    if (registered) return;

    // Geode namespaces and flattens declared resources into this directory.
    auto resourceDir = Mod::get()->getResourcesDir();
    std::size_t count = 0;

    for (auto const& asset : RobotAssets) {
        auto png = resourceDir / fmt::format("{}.png", asset.slug);
        auto plist = resourceDir / fmt::format("{}.plist", asset.slug);
        if (!std::filesystem::is_regular_file(png) || !std::filesystem::is_regular_file(plist)) {
            log::error("GeoBR Robots: recurso ausente para {}", asset.displayName);
            continue;
        }

        auto name = fmt::format("{}:{}", PackID, asset.slug);
        if (more_icons::addIcon(
            std::move(name),
            std::string(asset.displayName),
            IconType::Robot,
            std::move(png),
            std::move(plist),
            cocos2d::kTextureQualityLow,
            std::string(PackID),
            std::string(PackName)
        )) {
            ++count;
        }
    }

    registered = count == RobotAssets.size();
    if (registered) {
        log::info("GeoBR Robots: Sneaker Bot, Blue Runner e Orange Guard registrados");
    }
    else {
        log::error("GeoBR Robots: somente {}/{} robots foram registrados", count, RobotAssets.size());
    }
}

} // namespace geobr
