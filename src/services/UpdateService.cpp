#include "UpdateService.hpp"
#include "../utils/Sha256.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

#include <filesystem>
#include <memory>

using namespace geode::prelude;
using namespace geode::utils;

namespace geobr::update {
namespace {

constexpr char const* RepoRaw = "https://raw.githubusercontent.com/LZDLabs/GeoBrGDPS/main/";
constexpr char const* ManifestPath = "updates/latest.json";

#if defined(GEODE_IS_WINDOWS)
constexpr char const* Platform = "win";
#elif defined(GEODE_IS_ANDROID64)
constexpr char const* Platform = "android64";
#elif defined(GEODE_IS_ANDROID32)
constexpr char const* Platform = "android32";
#elif defined(GEODE_IS_MACOS)
constexpr char const* Platform = "mac";
#else
constexpr char const* Platform = "ios";
#endif

struct Package {
    std::string id, name, version, url, sha256;
    bool installed = false;
    std::string current;
};

async::TaskHolder<web::WebResponse> s_manifest;
async::TaskHolder<web::WebResponse> s_download;
bool s_checked = false;

// Android (Geode launcher, "patchless"): the game loads each mod's binary from
// the launcher's own folder, not from the .geode. Geode's own installer also
// puts the new binary in unzipped/binaries, which is taken over on the next
// start; this does exactly the same for a .geode the mod installed.
geode::Result<> stageBinary(std::filesystem::path const& geodePath) {
    if (!Loader::get()->isPatchless()) return geode::Ok();
    auto metadata = ModMetadata::createFromGeodeFile(geodePath);
    std::string binary(metadata.getBinaryName());
    if (binary.empty()) return geode::Err("pacote sem binario");
    GEODE_UNWRAP_INTO(auto unzip, file::Unzip::create(geodePath));
    if (!unzip.hasEntry(binary)) return geode::Err("binario " + binary + " nao esta no pacote");
    auto dir = dirs::getModRuntimeDir() / "binaries";
    (void)file::createDirectoryAll(dir);
    GEODE_UNWRAP(unzip.extractTo(binary, dir / binary));
    log::info("Update: binary {} staged for the next start", binary);
    return geode::Ok();
}

web::WebRequest request(int seconds) {
    web::WebRequest r;
    r.timeout(std::chrono::seconds{seconds}).certVerification(true).followRedirects(true);
    return r;
}

// Only files from the release repository are ever installed.
bool trustedUrl(std::string const& url) { return url.rfind(RepoRaw, 0) == 0 && url.find("..") == std::string::npos; }

std::vector<Package> newer(matjson::Value const& manifest) {
    std::vector<Package> out;
    auto packages = manifest["packages"].asArray();
    if (!packages) return out;
    for (auto const& p : packages.unwrap()) {
        Package pkg;
        pkg.id = p["id"].asString().unwrapOr("");
        pkg.name = p["name"].asString().unwrapOr(pkg.id);
        pkg.version = p["version"].asString().unwrapOr("");
        auto file = p["files"][Platform];
        pkg.url = file["url"].asString().unwrapOr("");
        pkg.sha256 = file["sha256"].asString().unwrapOr("");
        if ((pkg.id != "geobr.geobrmod" && pkg.id != "geobr.gdps") || pkg.url.empty() || pkg.sha256.size() != 64 || !trustedUrl(pkg.url)) continue;
        auto offered = VersionInfo::parse(pkg.version);
        if (!offered) continue;
        auto mod = Loader::get()->getInstalledMod(pkg.id);
        pkg.installed = mod != nullptr;
        if (mod) {
            pkg.current = mod->getVersion().toVString();
            if (!(offered.unwrap() > mod->getVersion())) continue;
        }
        out.push_back(std::move(pkg));
    }
    return out;
}

void finish(int installed, std::string error) {
    if (!error.empty()) {
        FLAlertLayer::create("Atualizacao GeoBR", fmt::format("A atualizacao parou: {}\nNada foi alterado no pacote que falhou.", error), "OK")->show();
        if (installed == 0) return;
    }
    createQuickPopup("Atualizacao GeoBR", "Pronto! A nova versao entra quando o jogo reiniciar.", "Depois", "Reiniciar",
        [](auto, bool restart) { if (restart) game::restart(true); });
}

void install(std::shared_ptr<std::vector<Package>> list, size_t index, int installed) {
    if (index >= list->size()) { finish(installed, {}); return; }
    auto const& pkg = (*list)[index];
    Notification::create(fmt::format("Baixando {} {}...", pkg.name, pkg.version), NotificationIcon::Loading, 2.f)->show();
    s_download.spawn("GeoBR - atualizacao", request(300).get(pkg.url), [list, index, installed](web::WebResponse response) {
        auto const& pkg = (*list)[index];
        if (!response.ok()) { finish(installed, fmt::format("{}: download falhou (HTTP {})", pkg.name, response.code())); return; }
        auto const& data = response.data();
        auto hash = sha256::hex(data.data(), data.size());
        if (hash != pkg.sha256) {
            log::error("Update {}: sha256 {} != {}", pkg.id, hash, pkg.sha256);
            finish(installed, fmt::format("{}: arquivo diferente do publicado (SHA-256)", pkg.name));
            return;
        }
        auto target = dirs::getModsDir() / (pkg.id + ".geode");
        auto temp = dirs::getModsDir() / (pkg.id + ".geode.download");
        if (auto written = file::writeBinary(temp, data); !written) {
            finish(installed, fmt::format("{}: nao foi possivel gravar ({})", pkg.name, written.unwrapErr()));
            return;
        }
        std::error_code error;
        std::filesystem::rename(temp, target, error);
        if (error) {
            std::filesystem::remove(temp, error);
            finish(installed, fmt::format("{}: nao foi possivel substituir o pacote", pkg.name));
            return;
        }
        if (auto staged = stageBinary(target); !staged) {
            finish(installed, fmt::format("{}: pacote salvo, mas o binario nao foi preparado ({})", pkg.name, staged.unwrapErr()));
            return;
        }
        log::info("Update: installed {} {} ({})", pkg.id, pkg.version, hash);
        install(list, index + 1, installed + 1);
    });
}

} // namespace

void repairStaleBinary(char const* compiledVersion) {
    // A .geode newer than the binary that is running (an update installed by an
    // older updater on Android): stage this package's binary and ask for a
    // full restart, so the new version really loads.
    static bool done = false;
    if (done) return;
    done = true;
    auto packaged = Mod::get()->getVersion().toVString();
    auto running = std::string("v") + compiledVersion;
    if (packaged == running || packaged == compiledVersion) return;
    log::warn("Update: package is {} but the running binary is {}", packaged, compiledVersion);
    if (auto staged = stageBinary(Mod::get()->getPackagePath()); !staged) {
        log::error("Update: could not stage binary: {}", staged.unwrapErr());
        return;
    }
    createQuickPopup("Atualizacao GeoBR",
        fmt::format("A GeoBR {} foi baixada, mas ainda esta rodando a {}.\nFeche o jogo por completo e abra de novo para terminar.", packaged, running),
        "Depois", "Reiniciar", [](auto, bool restart) { if (restart) game::restart(true); });
}

void checkOnce() {
    if (s_checked) return;
    s_checked = true;
    s_manifest.spawn("GeoBR - procurar atualizacao", request(20).get(std::string(RepoRaw) + ManifestPath), [](web::WebResponse response) {
        if (!response.ok()) { log::info("Update check: HTTP {}", response.code()); return; }
        auto manifest = response.json();
        if (!manifest) return;
        auto list = std::make_shared<std::vector<Package>>(newer(manifest.unwrap()));
        if (list->empty()) return;
        std::string key;
        std::string lines;
        for (auto const& p : *list) {
            key += p.id + "@" + p.version + ";";
            lines += p.installed ? fmt::format("{}: {} -> {}\n", p.name, p.current, p.version) : fmt::format("{} {} (novo)\n", p.name, p.version);
        }
        if (Mod::get()->getSavedValue<std::string>("update-skipped") == key) return;
        auto notes = manifest.unwrap()["notes"].asString().unwrapOr("");
        createQuickPopup("Atualizacao GeoBR", fmt::format("{}{}\nBaixar e instalar agora?", lines, notes.empty() ? "" : "\n" + notes.substr(0, 300) + "\n"),
            "Agora nao", "Atualizar", [list, key](auto, bool yes) {
                if (!yes) { Mod::get()->setSavedValue("update-skipped", key); return; }
                install(list, 0, 0);
            });
    });
}

} // namespace geobr::update
