#pragma once

#include "../models/MainLevel.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

#include <string>

class GJGameLevel;

namespace geobr {

struct ImportedMainLevel final {
    GJGameLevel* level = nullptr;
    std::string error;

    explicit operator bool() const { return level != nullptr; }
};

class MainLevelLauncher final {
public:
    static geode::utils::web::WebFuture download(MainLevel const& level);
    static ImportedMainLevel cached(MainLevel const& metadata);
    static ImportedMainLevel import(MainLevel const& metadata, geode::utils::web::WebResponse const& response);
};

}
