#include "GeoBRConfig.hpp"
#include <Geode/Geode.hpp>

#include <cctype>
#include <fmt/format.h>

using namespace geode::prelude;
namespace geobr {
namespace {

std::string trimTrailingSlashes(std::string value) {
    while (!value.empty() && value.back() == '/') value.pop_back();
    return value;
}

}

bool GeoBRConfig::useMockData() { return false; }
std::string GeoBRConfig::apiBaseUrl() {
    auto url = std::string(DefaultApiBaseUrl);
    if (url.empty()) url = std::string(DefaultApiBaseUrl);
    return trimTrailingSlashes(std::move(url));
}
std::string GeoBRConfig::endpointUrl(std::string_view endpoint) { return apiBaseUrl() + std::string(endpoint); }
std::string GeoBRConfig::supabaseUrl() {
    auto url = Mod::get()->getSettingValue<std::string>("supabase-url");
    if (url.empty()) url = std::string(DefaultSupabaseUrl);
    return trimTrailingSlashes(std::move(url));
}
std::string GeoBRConfig::supabasePublishableKey() {
    return Mod::get()->getSettingValue<std::string>("supabase-publishable-key");
}
std::string GeoBRConfig::supabaseTableUrl(std::string_view table) {
    return fmt::format("{}/rest/v1/{}", supabaseUrl(), table);
}
std::string GeoBRConfig::encodeStoragePath(std::string_view objectPath) {
    constexpr char Hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(objectPath.size());
    for (auto raw : objectPath) {
        auto byte = static_cast<unsigned char>(raw);
        if (std::isalnum(byte) || raw == '-' || raw == '_' || raw == '.' || raw == '/' || raw == '~') {
            encoded.push_back(raw);
        }
        else {
            encoded.push_back('%');
            encoded.push_back(Hex[(byte >> 4) & 0xf]);
            encoded.push_back(Hex[byte & 0xf]);
        }
    }
    return encoded;
}
bool GeoBRConfig::validStoragePath(std::string_view objectPath) {
    if (objectPath.empty() || objectPath.size() > 240 || objectPath.starts_with('/') ||
        objectPath.contains("..") || objectPath.contains("://") || objectPath.contains('\\')) return false;
    auto dot = objectPath.find_last_of('.');
    if (dot == std::string_view::npos) return false;
    auto extension = objectPath.substr(dot);
    return extension == ".gmd" || extension == ".gmd2" || extension == ".lvl";
}
std::string GeoBRConfig::supabaseStorageUrl(std::string_view objectPath) {
    return fmt::format(
        "{}/storage/v1/object/public/{}/{}",
        supabaseUrl(), MainLevelsBucket, encodeStoragePath(objectPath)
    );
}
std::string GeoBRConfig::supabaseConfigurationError() {
    auto url = supabaseUrl();
    if (!isSecureBackendUrl(url)) return "Configure uma URL HTTPS valida para o Supabase.";
    if (supabasePublishableKey().empty()) {
        return "Configure a Publishable Key do Supabase nas opcoes do GeoBR Mod.";
    }
    return {};
}
bool GeoBRConfig::isSecureBackendUrl(std::string_view url) {
    return url.starts_with("https://") || url.starts_with("http://localhost") || url.starts_with("http://127.0.0.1");
}
}
