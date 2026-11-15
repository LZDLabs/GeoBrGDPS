#pragma once
#include <Geode/utils/web.hpp>
#include <matjson.hpp>
#include <optional>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace geobr {
struct ApiJsonResult final {bool success=false;matjson::Value value;std::string error;int httpStatus=0;};
class ApiClient final {
public:
    using QueryParams=std::vector<std::pair<std::string,std::string>>;
    static geode::utils::web::WebFuture get(std::string_view, QueryParams const& = {});
    static geode::utils::web::WebFuture post(std::string_view,matjson::Value const&);
    static geode::utils::web::WebFuture getAuthorized(
        std::string, std::string_view, QueryParams const&, std::chrono::seconds
    );
    static geode::utils::web::WebFuture getSupabase(
        std::string, QueryParams const& = {}
    );
    static geode::utils::web::WebFuture getBinary(std::string);
    static geode::utils::web::WebFuture postAuthorized(
        std::string, std::string_view, matjson::Value const&, std::chrono::seconds
    );
    static geode::utils::web::WebFuture postSupabaseAuthorized(
        std::string, std::string_view, matjson::Value const&, std::string_view prefer = "return=representation"
    );
    static ApiJsonResult decodeJson(geode::utils::web::WebResponse const&);
    static std::optional<std::string> configurationError();
private:
    static void configure(geode::utils::web::WebRequest&, std::chrono::seconds);
};
}
