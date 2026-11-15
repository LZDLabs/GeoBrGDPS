#include "MainLevel.hpp"
#include "JsonModelUtils.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace geobr {

std::optional<MainLevel> MainLevel::fromJson(matjson::Value const& value) {
    if (!value.isObject()) return std::nullopt;

    auto name = jsonutil::stringField(value, "name");
    auto id = jsonutil::integerOr(value, "id");
    auto path = jsonutil::stringField(value, "gmd_path");
    if (!name || name->empty() || id <= 0 || id > 2'000'000'000 || !path || path->empty()) {
        return std::nullopt;
    }

    MainLevel level;
    level.id = id;
    level.order = static_cast<int>(std::max<std::int64_t>(0, jsonutil::integerOr(
        value, "campaign_order", jsonutil::integerOr(value, "order")
    )));
    level.name = std::move(*name);
    level.creator = jsonutil::stringOr(value, "creator", "Equipe GeoBR");
    level.difficulty = jsonutil::stringOr(value, "difficulty", "Sem dificuldade");
    level.description = jsonutil::stringOr(value, "description", "Main level oficial do GeoBR.");
    level.song = jsonutil::stringOr(value, "song", "Musica a definir");
    level.gmdPath = std::move(*path);
    level.stars = static_cast<int>(std::clamp<std::int64_t>(jsonutil::integerOr(value, "stars"), 0, 10));
    level.coins = static_cast<int>(std::clamp<std::int64_t>(jsonutil::integerOr(value, "coins"), 0, 3));
    level.fileVersion = static_cast<int>(std::clamp<std::int64_t>(jsonutil::integerOr(value, "file_version", 1), 1, 1'000'000));
    level.platformer = jsonutil::boolOr(value, "platformer", false);
    level.published = jsonutil::boolOr(value, "published", true);
    return level;
}

int MainLevel::difficultyCode() const {
    std::string key;
    key.reserve(difficulty.size());
    for (auto raw : difficulty) {
        auto byte = static_cast<unsigned char>(raw);
        if (std::isalnum(byte)) key.push_back(static_cast<char>(std::tolower(byte)));
    }
    static std::unordered_map<std::string, int> const Codes {
        {"auto", 0}, {"easy", 1}, {"facil", 1}, {"normal", 2},
        {"hard", 3}, {"dificil", 3}, {"harder", 4}, {"insane", 5},
        {"demon", 6}, {"easydemon", 7}, {"mediumdemon", 8},
        {"harddemon", 6}, {"insanedemon", 9}, {"extremedemon", 10},
    };
    if (auto found = Codes.find(key); found != Codes.end()) return found->second;
    return -1;
}

std::string MainLevel::modeLabel() const {
    return platformer ? "PLATAFORMA" : "CLASSICO";
}

}
