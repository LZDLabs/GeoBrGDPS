// GeoBR GDPS core (early-load, no dependencies).
//
// * Picks the server once per game session from the "server" setting.
// * GDPS GeoBR mode: GD's save files live in <save>/geobr-gdps/, so GD keeps
//   a completely separate CCGameManager.dat / CCLocalLevels.dat: separate
//   account, progress, created and saved levels (songs are shared).
//   The official save is never opened or written while in GDPS mode.
// * Redirects GD's server calls to the GeoBR GDPS while in GDPS mode. GeoBR
//   Music Library songs (IDs 8000001+) are answered by GeoBR in both modes.
//
// Loaded early because GD reads its save before normal mods load.
// Uninstalling this mod returns GD to the official server and the official save.
#include "gdps/GdpsRouting.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/CCHttpClient.hpp>
#include <Geode/modify/GManager.hpp>

#include <filesystem>

using namespace geode::prelude;

namespace {

constexpr char const* GdpsBase = "https://nolfgjsnnleqpmzvmebo.supabase.co/functions/v1/gdps";
constexpr char const* GdpsFolder = "geobr-gdps/";

// Decided once: switching servers always restarts the game.
bool gdpsSession() {
    static bool const active = [] {
        bool wants = Mod::get()->getSettingValue<std::string>("server") == "GDPS GeoBR";
        // The two mods go together: without GeoBR installed and enabled, GD stays official.
        auto geobrMod = Loader::get()->getInstalledMod("geobr.geobrmod");
        bool paired = geobrMod && geobrMod->isOrWillBeEnabled();
        if (wants && !paired) log::warn("GeoBR GDPS: o mod GeoBR nao esta instalado/ativo; o GD fica no servidor oficial");
        bool on = wants && paired;
        Mod::get()->setSavedValue<std::string>("active-server", on ? "gdps" : "oficial");
        log::info("GeoBR GDPS: server for this session: {}", on ? "GDPS GeoBR (separate save)" : "Oficial");
        return on;
    }();
    return active;
}

} // namespace

$on_mod(Loaded) {
    (void)gdpsSession();
}

// GameManager (CCGameManager.dat: account, progress, saved levels) and
// LocalLevelManager (CCLocalLevels.dat: created levels) both load and save
// through GManager::setup/save with m_fileName. In GDPS mode the name gets the
// geobr-gdps/ folder, on every platform (PC and Android), before GD reads it.
class $modify(GeoBRGdpsSave, GManager) {
    void setup() {
        if (gdpsSession()) {
            std::string name(m_fileName);
            if (!name.empty() && name.rfind(GdpsFolder, 0) != 0) {
                std::filesystem::path dir = std::string(CCFileUtils::sharedFileUtils()->getWritablePath());
                dir /= std::string(GdpsFolder).substr(0, std::string(GdpsFolder).size() - 1);
                std::error_code error;
                std::filesystem::create_directories(dir, error);
                // Never fall back to the official file: without the folder GD fails
                // to save instead of mixing the two servers.
                if (error) log::error("GeoBR GDPS: could not create {}: {}", dir.string(), error.message());
                m_fileName = gd::string(GdpsFolder + name);
                log::info("GeoBR GDPS: {} -> {}", name, std::string(m_fileName));
            }
        }
        GManager::setup();
    }
};

// Every GD request (levels, accounts, comments, chests...) goes through CCHttpClient::send.
class $modify(GeoBRGdpsHttp, cocos2d::extension::CCHttpClient) {
    void send(cocos2d::extension::CCHttpRequest* request) {
        if (request && request->getUrl()) {
            std::string_view body;
            if (request->getRequestData() && request->getRequestDataSize() > 0)
                body = std::string_view(request->getRequestData(), static_cast<std::size_t>(request->getRequestDataSize()));
            if (auto next = geobr::gdps::rewrite(request->getUrl(), body, gdpsSession(), GdpsBase))
                request->setUrl(next->c_str());
        }
        CCHttpClient::send(request);
    }
};
