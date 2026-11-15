#pragma once
#include "JsonModelUtils.hpp"

namespace geobr {
// matjson::Value() constructs {}, NOT null. A journal entry only exists when it
// carries a complete owner and request identity; empty legacy entries are idle.
inline bool isPendingShopOrder(matjson::Value const& entry) {
    auto args = entry.get("args");
    return entry.isObject() && !jsonutil::stringOr(entry, "owner").empty() &&
        args && args.unwrap().isObject() &&
        !jsonutil::stringOr(args.unwrap(), "p_item_id").empty() &&
        !jsonutil::stringOr(args.unwrap(), "p_request_id").empty();
}

inline matjson::Value shopArguments(matjson::Value const& item,
    matjson::Value const& state, std::string const& requestID) {
    matjson::Value run(nullptr), tier(nullptr);
    auto quest = state.get("quest"), season = state.get("season");
    if (quest && quest.unwrap().isObject()) {
        auto id = jsonutil::stringOr(quest.unwrap(), "run_id");
        if (!id.empty()) run = id;
    }
    if (season && season.unwrap().isObject()) {
        auto per = jsonutil::integerOr(season.unwrap(), "xp_per_tier", 100);
        if (per > 0) tier = jsonutil::integerOr(state, "xp") / per;
    }
    return matjson::makeObject({
        {"p_item_id", jsonutil::stringOr(item, "id")},
        {"p_request_id", requestID},
        {"p_expected_price", jsonutil::integerOr(item, "price")},
        {"p_expected_run", run}, {"p_expected_tier", tier}
    });
}
}
