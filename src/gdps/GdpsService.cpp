#include "GdpsService.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/AccessService.hpp"
#include "../services/CloudService.hpp"
#include "../services/GeoPassService.hpp"

#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;
using namespace geode::utils::web;

namespace geobr::gdps {

namespace {
Mod* core() { return Loader::get()->getLoadedMod("geobr.gdps"); }
}

bool coreInstalled() { return core() != nullptr; }

bool enabled() {
    auto mod = core();
    return mod && mod->getSavedValue<std::string>("active-server") == "gdps";
}

void switchServer(bool gdps) {
    auto mod = core();
    if (!mod) return;
    mod->setSettingValue<std::string>("server", gdps ? "GDPS GeoBR" : "Oficial");
    (void)mod->saveData();
    (void)Mod::get()->saveData();
    // Saves GD's data to the folder of the server being left, then restarts.
    utils::game::restart(true);
}
std::string baseUrl() { return GeoBRConfig::supabaseUrl() + "/functions/v1/gdps"; }

int accountID() {
    auto am = GJAccountManager::sharedState();
    return am ? am->m_accountID : 0;
}
std::string username() {
    auto am = GJAccountManager::sharedState();
    return am ? std::string(am->m_username) : std::string();
}
std::string gjp2() {
    auto am = GJAccountManager::sharedState();
    return am ? std::string(am->m_GJP2) : std::string();
}
bool loggedIn() { return enabled() && accountID() > 0 && !gjp2().empty(); }

namespace {
matjson::Value cachedMe = matjson::Value::object();
int cachedFor = 0;
Request& meRequest() {
    static Request request;
    return request;
}
}

matjson::Value const& me() {
    if (cachedFor != accountID()) { cachedMe = matjson::Value::object(); cachedFor = 0; }
    return cachedMe;
}

bool can(std::string_view permission) {
    auto const& m = me();
    if (jsonutil::boolOr(m, "owner", false)) return true;
    auto perms = m.get("permissions");
    if (!perms || !perms.unwrap().isArray()) return false;
    for (auto const& p : perms.unwrap().asArray().unwrap()) if (p.asString().unwrapOr("") == permission) return true;
    return false;
}

void refreshMe(std::function<void()> done) {
    if (!loggedIn()) { cachedMe = matjson::Value::object(); cachedFor = 0; if (done) done(); return; }
    int id = accountID();
    meRequest().post("me", matjson::Value::object(), [id, done = std::move(done)](Result r) {
        if (r.ok) { cachedMe = r.value; cachedFor = id; }
        if (done) done();
    });
}

void Request::cancel() {
    ++*m_generation;
    m_task.cancel();
}

void Request::post(std::string const& path, matjson::Value body, Callback cb, bool withAccount) {
    cancel();
    if (!body.isObject()) body = matjson::Value::object();
    if (withAccount) {
        if (!loggedIn()) {
            cb({.error = "Entre na sua conta GeoBR (Conta do GD) primeiro."});
            return;
        }
        body["accountID"] = accountID();
        body["gjp2"] = gjp2();
    }
    WebRequest request;
    request.header("Content-Type", "application/json").header("Accept", "application/json")
        .userAgent(std::string("GeoBR-Mod/") + ClientVersion)
        .timeout(std::chrono::seconds{25}).certVerification(true).followRedirects(false);
    request.bodyJSON(body);
    auto generation = m_generation;
    auto expected = *generation;
    m_task.spawn("GeoBR GDPS", request.post(baseUrl() + "/geobr/" + path), [generation, expected, cb = std::move(cb)](WebResponse response) {
        if (*generation != expected) return;
        Result result;
        auto json = response.json();
        if (json.isOk() && json.unwrap().isObject()) {
            result.value = json.unwrap();
            result.ok = jsonutil::boolOr(result.value, "ok", false);
            if (!result.ok) result.error = jsonutil::stringOr(result.value, "error");
        }
        if (!result.ok && result.error.empty()) {
            result.error = response.ok() ? "Resposta invalida do servidor GDPS."
                                         : fmt::format("Servidor GDPS indisponivel (HTTP {}).", response.code());
        }
        if (result.error.size() > 200) result.error.resize(200);
        cb(std::move(result));
    });
}

void Request::admin(std::string const& action, matjson::Value fields, Callback cb) {
    if (!fields.isObject()) fields = matjson::Value::object();
    fields["action"] = action;
    post("admin", std::move(fields), std::move(cb));
}

namespace {
Request& linkRequest() {
    static Request request;
    return request;
}
int linkAttempts = 0;
}

void linkProfileAfterLogin() {
    if (!loggedIn()) return;
    CloudService::authenticate([](CloudAuth auth) {
        if (!loggedIn()) return;
        auto body = matjson::makeObject({{"accessToken", auth.success ? auth.accessToken : std::string()}});
        linkRequest().post("session", body, [](Result r) {
            if (!r.ok) {
                log::warn("GeoBR GDPS: profile link failed: {}", r.error);
                Notification::create("Perfil GeoBR: " + r.error, NotificationIcon::Warning, 3.f)->show();
                return;
            }
            if (jsonutil::boolOr(r.value, "needs_new_profile", false)) {
                // This device's profile belongs to another GDPS account: start a fresh one.
                if (++linkAttempts > 2) return;
                CloudService::adoptSession("", "");
                linkProfileAfterLogin();
                return;
            }
            linkAttempts = 0;
            refreshMe();
            auto refresh = jsonutil::stringOr(r.value, "refresh_token");
            if (!refresh.empty()) {
                auto& saved = *Mod::get();
                if (saved.getSavedValue<std::string>("cloud-guest-refresh-token").empty())
                    saved.setSavedValue("cloud-guest-refresh-token", CloudService::refreshToken());
                CloudService::adoptSession(refresh, jsonutil::stringOr(r.value, "user_id"));
                GeoPassService::get()->refresh();
                Notification::create("Perfil GeoBR da conta carregado", NotificationIcon::Success, 2.f)->show();
            } else if (jsonutil::boolOr(r.value, "linked", false)) {
                Notification::create("Conta GDPS ligada ao seu perfil GeoBR", NotificationIcon::Success, 2.f)->show();
            }
        });
    });
}

void restoreGuestProfile() {
    auto guest = Mod::get()->getSavedValue<std::string>("cloud-guest-refresh-token");
    if (guest.empty()) return;
    Mod::get()->setSavedValue("cloud-guest-refresh-token", std::string());
    CloudService::adoptSession(guest, "");
    GeoPassService::get()->refresh();
}

} // namespace geobr::gdps
