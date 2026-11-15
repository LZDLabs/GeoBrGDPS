#pragma once
// GDPS request routing (pure logic, tested on the host by tests/GdpsRoutingTests.cpp).
//
// GD sends every server call to https://www.boomlings.com/database/<endpoint>.
// With the GeoBR GDPS on (companion mod geobr.gdps), those calls go to the
// `gdps` Edge Function instead, except the official song catalogue, the
// artists list, vault codes and the content CDN. GeoBR Music Library songs
// (IDs 8000001..9999999) are always answered by GeoBR: no Newgrounds song uses
// that range.
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace geobr::gdps {

inline constexpr int SongMin = 8000001;
inline constexpr int SongMax = 9999999;
inline bool isGeoBRSong(long long id) { return id >= SongMin && id <= SongMax; }

/** "https://www.boomlings.com/database/getGJLevels21.php" -> "getGJLevels21.php" (empty if not RobTop's server). */
inline std::string_view robtopEndpoint(std::string_view url) {
    for (std::string_view prefix : {"https://www.boomlings.com/database/", "http://www.boomlings.com/database/",
                                    "https://boomlings.com/database/", "http://boomlings.com/database/"}) {
        if (url.size() > prefix.size() && url.substr(0, prefix.size()) == prefix) return url.substr(prefix.size());
    }
    return {};
}

/** Value of `key` in an x-www-form-urlencoded body ("a=1&songID=5"). */
inline std::string_view formValue(std::string_view body, std::string_view key) {
    std::size_t pos = 0;
    while (pos <= body.size()) {
        auto end = body.find('&', pos);
        if (end == std::string_view::npos) end = body.size();
        auto pair = body.substr(pos, end - pos);
        auto eq = pair.find('=');
        if (eq != std::string_view::npos && pair.substr(0, eq) == key) return pair.substr(eq + 1);
        pos = end + 1;
    }
    return {};
}

inline long long formInt(std::string_view body, std::string_view key) {
    auto v = formValue(body, key);
    long long n = 0;
    auto r = std::from_chars(v.data(), v.data() + v.size(), n);
    return (r.ec == std::errc{} && r.ptr == v.data() + v.size()) ? n : 0;
}

/** Endpoints that stay on RobTop's server even with the GDPS on. */
inline bool staysOnRobTop(std::string_view endpoint) {
    for (std::string_view keep : {"getGJSecretReward.php", "getGJTopArtists.php", "getCustomContentURL.php"}) {
        if (endpoint == keep) return true;
    }
    return false;
}

/** New URL for a GD request, or nothing to leave it untouched. `base` has no trailing slash. */
inline std::optional<std::string> rewrite(std::string_view url, std::string_view body, bool gdpsEnabled, std::string_view base) {
    auto endpoint = robtopEndpoint(url);
    if (endpoint.empty() || base.empty()) return std::nullopt;
    auto query = endpoint.find('?');
    auto name = query == std::string_view::npos ? endpoint : endpoint.substr(0, query);
    if (name == "getGJSongInfo.php") {
        if (!isGeoBRSong(formInt(body, "songID"))) return std::nullopt;
    } else if (!gdpsEnabled || staysOnRobTop(name)) {
        return std::nullopt;
    }
    std::string out(base);
    out += '/';
    out += endpoint;
    return out;
}

} // namespace geobr::gdps
