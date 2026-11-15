#pragma once

#include <Geode/Geode.hpp>
#include <Geode/loader/Dispatch.hpp>

#include <filesystem>
#include <string>
#include <utility>

class IconInfo;

// Small, source-local adapter for the public More Icons event export. Keeping
// the adapter here avoids linking against another mod's binary while retaining
// the exact ABI of its documented addIcon API.
namespace more_icons {

inline IconInfo* addIcon(
    std::string name,
    std::string shortName,
    IconType type,
    std::filesystem::path png,
    std::filesystem::path plist,
    cocos2d::TextureQuality quality,
    std::string packID = {},
    std::string packName = "More Icons",
    bool vanilla = false,
    bool zipped = false
) {
    static auto storage = geode::geode_internal::callEventExportListener(
        &addIcon,
        "hiimjustin000.more_icons/addIcon"
    );
    if (!storage) return nullptr;

    return storage(
        std::move(name),
        std::move(shortName),
        type,
        std::move(png),
        std::move(plist),
        quality,
        std::move(packID),
        std::move(packName),
        vanilla,
        zipped
    );
}

} // namespace more_icons
