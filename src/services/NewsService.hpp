#pragma once
#include "ServiceResult.hpp"
#include "../models/NewsItem.hpp"
#include <Geode/utils/web.hpp>
#include <vector>
namespace geobr {class NewsService final{public:static geode::utils::web::WebFuture fetch();static ServiceResult<std::vector<NewsItem>> parse(geode::utils::web::WebResponse const&);};}
