#include "MainLevelsService.hpp"
#include "ApiClient.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../models/JsonModelUtils.hpp"

#include <algorithm>
#include <Geode/Geode.hpp>

namespace geobr {
namespace {

void sortByCampaignOrder(std::vector<MainLevel>& levels) {
    std::ranges::stable_sort(levels, [](MainLevel const& first, MainLevel const& second) {
        return first.order < second.order;
    });
}

}

geode::utils::web::WebFuture MainLevelsService::fetch() {
    return ApiClient::getSupabase(
        GeoBRConfig::supabaseTableUrl(GeoBRConfig::MainLevelsTable),
        {
            {"select", "id,campaign_order,name,creator,description,difficulty,stars,coins,song,platformer,gmd_path,file_version,published"},
            {"published", "eq.true"},
            {"order", "campaign_order.asc,id.asc"},
        }
    );
}

ServiceResult<std::vector<MainLevel>> MainLevelsService::parse(
    geode::utils::web::WebResponse const& response
) {
    auto decoded = ApiClient::decodeJson(response);
    if (!decoded.success) return ServiceResult<std::vector<MainLevel>>::fail(std::move(decoded.error));

    auto payload = jsonutil::arrayPayload(decoded.value, "levels");
    if (!payload) return ServiceResult<std::vector<MainLevel>>::fail("Formato de main levels invalido.");
    auto array = payload->asArray();
    if (array.isErr()) return ServiceResult<std::vector<MainLevel>>::fail("Formato de main levels invalido.");

    auto values = array.unwrap();
    std::vector<MainLevel> levels;
    for (auto const& value : values) {
        if (auto level = MainLevel::fromJson(value)) levels.push_back(std::move(*level));
    }
    if (!values.empty() && levels.empty()) {
        return ServiceResult<std::vector<MainLevel>>::fail("O servidor nao enviou main levels validos.");
    }
    geode::Mod::get()->setSavedValue("campaign-catalog",decoded.value);
    sortByCampaignOrder(levels);
    return ServiceResult<std::vector<MainLevel>>::ok(std::move(levels));
}

std::vector<MainLevel> MainLevelsService::cached(){
 auto v=geode::Mod::get()->getSavedValue<matjson::Value>("campaign-catalog",matjson::Value::array());std::vector<MainLevel> levels;
 if(v.isArray())for(auto const& row:v.asArray().unwrap())if(auto level=MainLevel::fromJson(row))levels.push_back(*level);
 sortByCampaignOrder(levels);return levels;
}

}
