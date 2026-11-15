#pragma once
#include <matjson.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace geobr::jsonutil {
inline std::optional<std::string> stringField(matjson::Value const& object, std::string_view key) {
    auto valueResult = object.get(std::string(key));
    if (!valueResult) return std::nullopt;
    auto const& value = valueResult.unwrap();
    if (!value.isString()) return std::nullopt;
    auto result = value.asString();
    if (result.isErr()) return std::nullopt;
    return result.unwrap();
}
inline std::string stringOr(matjson::Value const& object, std::string_view key, std::string fallback = {}) {
    auto value = stringField(object, key); return value ? *value : std::move(fallback);
}
inline std::int64_t integerOr(matjson::Value const& object, std::string_view key, std::int64_t fallback = 0) {
    auto result = object.get(std::string(key)); if (!result) return fallback;
    auto const& value = result.unwrap(); if (!value.isNumber()) return fallback;
    auto number = value.asInt(); return number.isOk() ? number.unwrap() : fallback;
}
inline bool boolOr(matjson::Value const& object, std::string_view key, bool fallback = false) {
    auto result = object.get(std::string(key)); if (!result) return fallback;
    auto const& value = result.unwrap(); if (!value.isBool()) return fallback;
    auto flag = value.asBool(); return flag.isOk() ? flag.unwrap() : fallback;
}
inline std::optional<matjson::Value> arrayPayload(matjson::Value const& root, std::string_view key) {
    if (root.isArray()) return root;
    if (!root.isObject()) return std::nullopt;
    if (auto direct = root.get(std::string(key)); direct && direct.unwrap().isArray()) return direct.unwrap();
    if (auto data = root.get("data"); data && data.unwrap().isArray()) return data.unwrap();
    return std::nullopt;
}
}
