#include "TextSanitizer.hpp"
#include <algorithm>
namespace geobr::text {
namespace {
// Length of a UTF-8 sequence from its lead byte; 0 for bytes that can never
// start a sequence (stray continuation bytes, 0xF8+). RC5 copied those bytes
// through, producing invalid UTF-8 that the server or bitmap fonts reject.
std::size_t length(unsigned char c) {
    if ((c & 0x80u) == 0) return 1;
    if ((c & 0xE0u) == 0xC0u) return c >= 0xC2u ? 2 : 0;  // C0/C1 are overlong
    if ((c & 0xF0u) == 0xE0u) return 3;
    if ((c & 0xF8u) == 0xF0u) return c <= 0xF4u ? 4 : 0;
    return 0;
}
bool cont(unsigned char c) { return (c & 0xC0u) == 0x80u; }
}
std::string truncateUtf8(std::string_view in, std::size_t max) {
    std::string out;
    std::size_t p = 0, n = 0;
    while (p < in.size() && n < max) {
        auto l = length(static_cast<unsigned char>(in[p]));
        if (l == 0 || p + l > in.size()) { ++p; continue; }
        bool ok = true;
        for (std::size_t i = 1; i < l; ++i) if (!cont(static_cast<unsigned char>(in[p + i]))) ok = false;
        if (!ok) { ++p; continue; }
        out.append(in.substr(p, l));
        p += l;
        ++n;
    }
    return out;
}
std::string sanitizeMessage(std::string_view in, std::size_t max) {
    std::string out;
    bool space = false;
    for (unsigned char c : in) {
        if (c == '<' || c == '>') continue;
        if (c < 0x20u || c == 0x7Fu) {
            if (c == '\n' || c == '\r' || c == '\t') space = !out.empty();
            continue;
        }
        if (c == ' ') { space = !out.empty(); continue; }
        if (space) { out.push_back(' '); space = false; }
        out.push_back(static_cast<char>(c));
    }
    return truncateUtf8(out, max);
}
}
