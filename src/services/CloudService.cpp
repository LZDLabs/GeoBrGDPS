#include "CloudService.hpp"
#include "AccessService.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../models/JsonModelUtils.hpp"
#include <Geode/Geode.hpp>
#include <ctime>
using namespace geode::prelude;
using namespace geode::utils::web;
namespace geobr {
namespace {
CloudAuth session;
std::time_t expires=0;
bool authenticating=false;
async::TaskHolder<WebResponse> authTask;
std::vector<CloudService::AuthCallback> waiting;
}
std::string CloudService::userID(){return Mod::get()->getSavedValue<std::string>("cloud-user-id");}
std::string CloudService::refreshToken(){return Mod::get()->getSavedValue<std::string>("cloud-refresh-token");}
void CloudService::adoptSession(std::string refreshToken,std::string userID){
    // A request already in flight finishes with the old profile; the next one uses the new.
    session=CloudAuth{};expires=0;
    Mod::get()->setSavedValue("cloud-refresh-token",refreshToken);
    Mod::get()->setSavedValue("cloud-user-id",userID);
    if(auto saved=Mod::get()->saveData();saved.isErr())log::warn("GeoBR: could not persist the switched session");
}
ApiJsonResult CloudService::decode(WebResponse const& response) {
    if (!response.ok()) {
        std::string error;
        if (auto json=response.json();json.isOk()) for (auto key:{"message","msg","error_description","error"}) {
            error=jsonutil::stringOr(json.unwrap(),key);
            if (!error.empty()) break;
        }
        if (error.empty()) error=fmt::format("Falha de conexao (HTTP {}). Tente novamente.",response.code());
        if (error.size()>200) error.resize(200);
        return {.error=error,.httpStatus=static_cast<int>(response.code())};
    }
    if (response.data().empty()) return {true,matjson::Value::object(),{}};
    return ApiClient::decodeJson(response);
}
void CloudService::authenticate(AuthCallback cb) {
    if (session.success && expires>std::time(nullptr)+60){cb(session);return;}
    waiting.push_back(std::move(cb));
    if (authenticating) return;
    authenticating=true;
    auto refresh=Mod::get()->getSavedValue<std::string>("cloud-refresh-token");
    WebRequest request;
    request.header("apikey",GeoBRConfig::supabasePublishableKey()).header("Content-Type","application/json")
        .timeout(std::chrono::seconds{15}).certVerification(true).followRedirects(false);
    request.bodyJSON(refresh.empty()?matjson::Value::object():matjson::makeObject({{"refresh_token",refresh}}));
    authTask.spawn("GeoBR - sessao",request.post(GeoBRConfig::supabaseUrl()+
        (refresh.empty()?"/auth/v1/signup":"/auth/v1/token?grant_type=refresh_token")),[hadRefresh=!refresh.empty()](auto response){
        auto result=decode(response);CloudAuth next;
        if (result.success) {
            next.accessToken=jsonutil::stringOr(result.value,"access_token");
            next.refreshToken=jsonutil::stringOr(result.value,"refresh_token");
            if (auto user=result.value.get("user");user) next.userID=jsonutil::stringOr(user.unwrap(),"id");
            next.success=!next.accessToken.empty()&&!next.refreshToken.empty()&&!next.userID.empty();
            expires=std::time(nullptr)+jsonutil::integerOr(result.value,"expires_in",3600);
        }
        if (!next.success) next.error=result.error.empty()?"Sessao invalida. Tente novamente.":result.error;
        if (!next.success && hadRefresh && response.code()==400)
            next.error="Sessao expirada. Contate o ADM para recuperar seu acesso.";
        if (next.success) {
            Mod::get()->setSavedValue("cloud-refresh-token",next.refreshToken);
            Mod::get()->setSavedValue("cloud-user-id",next.userID);
            if(auto saved=Mod::get()->saveData();saved.isErr()) {
                next.success=false;next.error="Nao foi possivel salvar a sessao neste aparelho.";
                log::warn("GeoBR: could not persist authentication session");
            } else session=next;
        }
        authenticating=false;
        auto callbacks=std::move(waiting);waiting.clear();
        for (auto& callback:callbacks) callback(next);
    });
}
WebFuture CloudService::request(std::string method,std::string path,std::string token,
    matjson::Value body,bool binary,std::vector<uint8_t> bytes) {
    WebRequest request;
    request.header("apikey",GeoBRConfig::supabasePublishableKey()).header("Authorization","Bearer "+token)
        .header("Accept","application/json").header("Prefer","return=representation")
        .header("x-geobr-version",ClientVersion).userAgent(std::string("GeoBR-Mod/")+ClientVersion).certVerification(true).followRedirects(false)
        .timeout(std::chrono::seconds{binary?60:15});
    if (method!="GET"&&method!="DELETE") {
        if (binary) request.header("Content-Type","application/octet-stream").body(std::move(bytes));
        else request.header("Content-Type","application/json").bodyJSON(body);
    }
    return request.send(method,GeoBRConfig::supabaseUrl()+path);
}
void CloudRequest::cancel(){++*m_generation;m_task.cancel();}
void CloudRequest::call(std::string method,std::string path,matjson::Value body,Callback cb,bool binary,std::vector<uint8_t> bytes) {
    cancel();auto generation=m_generation;auto expected=*generation;
    CloudService::authenticate([this,generation,expected,method=std::move(method),path=std::move(path),body=std::move(body),cb=std::move(cb),binary,bytes=std::move(bytes)](CloudAuth auth) mutable {
        if (*generation!=expected)return;
        if (!auth.success){cb({.error=auth.error});return;}
        m_task.spawn("GeoBR - nuvem",CloudService::request(std::move(method),std::move(path),std::move(auth.accessToken),std::move(body),binary,std::move(bytes)),
            [cb=std::move(cb)](auto response){cb(CloudService::decode(response));});
    });
}
}
