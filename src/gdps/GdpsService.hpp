#pragma once
// GeoBR GDPS client side: settings, the logged-in GD account and the JSON
// endpoints of the `gdps` Edge Function (/geobr/...). The server checks the
// account (accountID + gjp2) and every permission on each request.
#include "GdpsRouting.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
#include <functional>
#include <memory>
#include <string>

namespace geobr::gdps {

/** The companion mod geobr.gdps (separate saves + redirect) is installed and loaded. */
bool coreInstalled();
/** This game session runs on the GeoBR GDPS (decided at startup by geobr.gdps). */
bool enabled();
/** Saves GD, switches the server in geobr.gdps and restarts the game. */
void switchServer(bool gdps);
std::string baseUrl();
int accountID();
std::string username();
std::string gjp2();
/** On the GDPS and logged in on GD (the account in the GDPS save is a GDPS account). */
bool loggedIn();
/** Cached /geobr/me of the logged-in account (roles, permissions). Empty until loaded. */
matjson::Value const& me();
bool can(std::string_view permission);
/** Reloads me(); `done` runs on the main thread when it arrives (even on failure). */
void refreshMe(std::function<void()> done = {});

struct Result {
    bool ok = false;
    matjson::Value value = matjson::Value::object();
    std::string error;
};

// One request at a time per owner; destroying/cancelling drops the callback,
// so a closed popup is never touched.
class Request {
    std::shared_ptr<int> m_generation = std::make_shared<int>(0);
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_task;
public:
    using Callback = std::function<void(Result)>;
    Request() = default;
    Request(Request const&) = delete;
    ~Request() { cancel(); }
    void cancel();
    /** POST /geobr/<path>; `withAccount` adds accountID + gjp2. */
    void post(std::string const& path, matjson::Value body, Callback cb, bool withAccount = true);
    /** Admin action: {"action": name, ...fields}. */
    void admin(std::string const& action, matjson::Value fields, Callback cb);
};

/** After a GD login: links the GDPS account to this device's GeoBR profile,
 *  or loads the profile already linked to the account (Pass, Collab, etc.). */
void linkProfileAfterLogin();
/** After a GD logout: back to the guest profile this device had. */
void restoreGuestProfile();

} // namespace geobr::gdps
