#pragma once
#include "ApiClient.hpp"
#include <Geode/utils/async.hpp>
#include <functional>
#include <memory>
namespace geobr {
struct CloudAuth { bool success=false; std::string accessToken, refreshToken, userID, error; };
class CloudService {
public:
    using AuthCallback=std::function<void(CloudAuth)>;
    static void authenticate(AuthCallback);
    static std::string userID();
    // GDPS: switch this device to another GeoBR profile (empty token = new guest profile).
    static void adoptSession(std::string refreshToken, std::string userID);
    static std::string refreshToken();
    static geode::utils::web::WebFuture request(std::string method, std::string path, std::string token,
        matjson::Value body={}, bool binary=false, std::vector<uint8_t> bytes={});
    static ApiJsonResult decode(geode::utils::web::WebResponse const&);
};
// The generation token outlives the UI: an authentication completion never dereferences a closed popup.
class CloudRequest {
    std::shared_ptr<int> m_generation=std::make_shared<int>(0);
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_task;
public:
    using Callback=std::function<void(ApiJsonResult)>;
    CloudRequest()=default;
    CloudRequest(CloudRequest const&)=delete;
    ~CloudRequest(){cancel();}
    void cancel();
    void call(std::string method, std::string path, matjson::Value body, Callback,
        bool binary=false, std::vector<uint8_t> bytes={});
    void rpc(std::string fn, matjson::Value body, Callback cb) {
        call("POST", "/rest/v1/rpc/"+fn, std::move(body), std::move(cb));
    }
};
}
