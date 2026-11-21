#pragma once
#include <matjson.hpp>
#include <optional>
#include <string>
namespace geobr {
enum class NewsType { News, Notice, Event, Maintenance, Update };
struct NewsItem final {
    std::string id, title, description, date, author, imagePath;
    bool isNew = false; int priority = 0; bool pinned = false; NewsType type = NewsType::News;
    static std::optional<NewsItem> fromJson(matjson::Value const& value);
    [[nodiscard]] std::string typeLabel() const;
};
}
