#pragma once
#include <Geode/utils/function.hpp>
#include <Geode/utils/permission.hpp>
#include <string>
#include <utility>

// Android 11+: the Geode launcher only opens its file picker after the user grants
// "Acesso a todos os arquivos" (MANAGE_EXTERNAL_STORAGE). Without it the launcher
// sends the user to Settings and file::pick returns Err("Permission error").
// Asking up front lets us explain why and open the picker right after it is granted.
namespace geobr::ui {

inline void ensureFileAccess(geode::Function<void(bool)> then) {
#ifdef GEODE_IS_ANDROID
    using geode::utils::permission::Permission;
    if (!geode::utils::permission::getPermissionStatus(Permission::ReadAllFiles)) {
        geode::utils::permission::requestPermission(Permission::ReadAllFiles, std::move(then));
        return;
    }
#endif
    then(true);
}

inline constexpr char const* FileAccessDenied =
    "Libere 'Acesso a todos os arquivos' para o Geode nas configuracoes do Android, ou use 'Da pasta'.";

// Turns a file::pick error into something the player can act on.
inline std::string pickErrorMessage(std::string const& error) {
    if (error.find("Permission") != std::string::npos) {
#ifdef GEODE_IS_ANDROID
        // The launcher also reports this when it cannot turn the chosen file into a
        // real path (Download subfolders, SD card, Drive, Telegram) or when cancelled.
        return "Nenhum arquivo lido. Se escolheu um, mova-o para Download (sem subpasta) ou use 'Da pasta'.";
#else
        return "Sem permissao para abrir o arquivo.";
#endif
    }
    if (error.find("already called") != std::string::npos) return "O seletor ja esta aberto.";
    return "Nao foi possivel abrir o seletor: " + error;
}

} // namespace geobr::ui
