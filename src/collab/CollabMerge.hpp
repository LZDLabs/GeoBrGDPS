#pragma once
// GeoBR collab: three-way merge of GD level strings (pure, host-testable).
//
// A level string is "header;object;object;...;" and each object is a list of
// key,value pairs. Two people editing the same room start from the same base
// (the last revision they synced). Merging works on objects as text:
//   result = theirs - (objects I removed) + (objects I added)
// so concurrent additions from both sides are kept, a deletion only removes
// the copy that was deleted, and moving an object (= remove + add) works.
// The header (level settings) comes from whoever changed it; when both did,
// mine wins.
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace geobr::collab {

struct SplitLevel {
    std::string header;
    std::vector<std::string> objects;
};

inline SplitLevel splitLevel(std::string_view level) {
    SplitLevel out;
    size_t start = 0;
    bool first = true;
    while (start <= level.size()) {
        auto end = level.find(';', start);
        if (end == std::string_view::npos) end = level.size();
        auto part = level.substr(start, end - start);
        if (first) { out.header = std::string(part); first = false; }
        else if (!part.empty()) out.objects.emplace_back(part);
        if (end == level.size()) break;
        start = end + 1;
    }
    return out;
}

inline std::string joinLevel(SplitLevel const& level) {
    std::string out = level.header;
    out += ';';
    for (auto const& object : level.objects) { out += object; out += ';'; }
    return out;
}

struct MergeResult {
    std::string level;
    int addedByMe = 0;
    int removedByMe = 0;
    bool headerFromMe = false;
};

inline MergeResult mergeLevels(std::string_view base, std::string_view mine, std::string_view theirs) {
    auto b = splitLevel(base), m = splitLevel(mine), t = splitLevel(theirs);
    std::unordered_map<std::string, int> baseCount, mineCount;
    for (auto const& o : b.objects) ++baseCount[o];
    for (auto const& o : m.objects) ++mineCount[o];
    // removed[o] = how many copies of o I deleted; added = copies I created.
    std::unordered_map<std::string, int> removed, added;
    for (auto const& [o, n] : baseCount) {
        auto it = mineCount.find(o);
        int have = it == mineCount.end() ? 0 : it->second;
        if (have < n) removed[o] = n - have;
    }
    for (auto const& [o, n] : mineCount) {
        auto it = baseCount.find(o);
        int had = it == baseCount.end() ? 0 : it->second;
        if (n > had) added[o] = n - had;
    }
    MergeResult result;
    SplitLevel out;
    bool headerChangedByMe = m.header != b.header;
    out.header = headerChangedByMe || t.header.empty() ? m.header : t.header;
    result.headerFromMe = headerChangedByMe;
    out.objects.reserve(t.objects.size() + m.objects.size());
    for (auto const& o : t.objects) {
        auto it = removed.find(o);
        if (it != removed.end() && it->second > 0) { --it->second; ++result.removedByMe; continue; }
        out.objects.push_back(o);
    }
    // Keep my additions in the order I made them.
    for (auto const& o : m.objects) {
        auto it = added.find(o);
        if (it != added.end() && it->second > 0) { --it->second; out.objects.push_back(o); ++result.addedByMe; }
    }
    result.level = joinLevel(out);
    return result;
}

// Header key that marks a level as belonging to a collab room.
inline constexpr char RoomKey[] = "kGBC";

inline std::string roomFromHeader(std::string_view header) {
    // header = key,value,key,value,...
    size_t pos = 0;
    while (pos < header.size()) {
        auto comma = header.find(',', pos);
        if (comma == std::string_view::npos) return {};
        auto key = header.substr(pos, comma - pos);
        auto valueEnd = header.find(',', comma + 1);
        if (valueEnd == std::string_view::npos) valueEnd = header.size();
        if (key == RoomKey) return std::string(header.substr(comma + 1, valueEnd - comma - 1));
        pos = valueEnd + 1;
    }
    return {};
}

inline bool validRoomID(std::string_view id) {
    if (id.size() != 36) return false;
    for (size_t i = 0; i < id.size(); ++i) {
        char c = id[i];
        bool dash = i == 8 || i == 13 || i == 18 || i == 23;
        if (dash ? c != '-' : !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

// Sets (or replaces) the room key in a level string's header.
inline std::string withRoom(std::string_view level, std::string_view room) {
    auto split = splitLevel(level);
    std::string header;
    size_t pos = 0;
    auto const& h = split.header;
    while (pos < h.size()) {
        auto comma = h.find(',', pos);
        if (comma == std::string::npos) { header += h.substr(pos); break; }
        auto valueEnd = h.find(',', comma + 1);
        if (valueEnd == std::string::npos) valueEnd = h.size();
        if (h.compare(pos, comma - pos, RoomKey) != 0) {
            if (!header.empty()) header += ',';
            header += h.substr(pos, valueEnd - pos);
        }
        pos = valueEnd + 1;
    }
    if (!room.empty()) {
        if (!header.empty()) header += ',';
        header += RoomKey; header += ','; header += room;
    }
    split.header = header;
    return joinLevel(split);
}

} // namespace geobr::collab
