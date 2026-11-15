#pragma once

#include "ServiceResult.hpp"
#include "../models/MainLevel.hpp"
#include <Geode/utils/web.hpp>
#include <vector>

namespace geobr {

class MainLevelsService final {
public:
    static geode::utils::web::WebFuture fetch();
    static std::vector<MainLevel> cached();
    static ServiceResult<std::vector<MainLevel>> parse(geode::utils::web::WebResponse const& response);
};

}
