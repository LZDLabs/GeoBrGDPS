#pragma once
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
namespace geobr {
class GeoBRConfig final {
public:
    static constexpr std::string_view DefaultApiBaseUrl = "https://api.geobr.invalid";
    static constexpr std::string_view DefaultSupabaseUrl = "https://nolfgjsnnleqpmzvmebo.supabase.co";
    static constexpr std::string_view MainLevelsTable = "geobr_main_levels";
    static constexpr std::string_view CommunityTable = "geobr_community_messages";
    static constexpr std::string_view MainLevelsBucket = "geobr-main-levels";
    static constexpr std::string_view NewsEndpoint = "/api/news";
    static constexpr std::string_view CommunityMessagesEndpoint = "/api/community/messages";
    static constexpr std::string_view MainLevelsEndpoint = "/api/main-levels";
    static constexpr std::size_t MaxMessageCodepoints = 160;
    static constexpr std::size_t MaxResponseBytes = 4 * 1024 * 1024;
    static constexpr std::size_t MaxGmdBytes = 12 * 1024 * 1024;
    static constexpr std::chrono::seconds RequestTimeout {10};
    static constexpr std::chrono::seconds MessageCooldown {3};
    static bool useMockData();
    static std::string apiBaseUrl();
    static std::string endpointUrl(std::string_view endpoint);
    static std::string supabaseUrl();
    static std::string supabasePublishableKey();
    static std::string supabaseTableUrl(std::string_view table);
    static std::string supabaseStorageUrl(std::string_view objectPath);
    static std::string encodeStoragePath(std::string_view objectPath);
    static bool validStoragePath(std::string_view objectPath);
    static std::string supabaseConfigurationError();
    static bool isSecureBackendUrl(std::string_view url);
};
}
