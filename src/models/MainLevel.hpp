#pragma once

#include <matjson.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace geobr {

struct MainLevel final {
    std::int64_t id = 0;
    int order = 0;
    std::string name;
    std::string creator;
    std::string difficulty;
    std::string description;
    std::string song;
    std::string gmdPath;
    int stars = 0;
    int coins = 0;
    int fileVersion = 1;
    bool platformer = false;
    bool published = false;

    static std::optional<MainLevel> fromJson(matjson::Value const& value);
    int difficultyCode() const;
    std::string modeLabel() const;
};

}
