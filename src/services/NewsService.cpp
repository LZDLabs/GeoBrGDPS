#include "NewsService.hpp"
#include "ApiClient.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../models/JsonModelUtils.hpp"
#include <algorithm>
namespace geobr {namespace{void sort(std::vector<NewsItem>&v){std::ranges::stable_sort(v,[](auto const&a,auto const&b){if(a.pinned!=b.pinned)return a.pinned>b.pinned;return a.priority>b.priority;});}}
geode::utils::web::WebFuture NewsService::fetch(){return ApiClient::getSupabase(GeoBRConfig::supabaseTableUrl("geobr_bulletins"),{{"published","eq.true"},{"order","pinned.desc,priority.desc,created_at.desc"},{"limit","50"}});}
ServiceResult<std::vector<NewsItem>> NewsService::parse(geode::utils::web::WebResponse const&r){auto d=ApiClient::decodeJson(r);if(!d.success)return ServiceResult<std::vector<NewsItem>>::fail(std::move(d.error));auto p=jsonutil::arrayPayload(d.value,"news");if(!p)return ServiceResult<std::vector<NewsItem>>::fail("Formato de noticias invalido.");auto a=p->asArray();if(a.isErr())return ServiceResult<std::vector<NewsItem>>::fail("Formato de noticias invalido.");auto values=a.unwrap();std::vector<NewsItem> out;for(auto const&v:values)if(auto x=NewsItem::fromJson(v))out.push_back(std::move(*x));if(!values.empty()&&out.empty())return ServiceResult<std::vector<NewsItem>>::fail("Nenhuma noticia valida foi recebida.");sort(out);return ServiceResult<std::vector<NewsItem>>::ok(std::move(out));}
}
