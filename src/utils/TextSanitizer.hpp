#pragma once
#include <cstddef>
#include <string>
#include <string_view>
namespace geobr::text { std::string sanitizeMessage(std::string_view,std::size_t); std::string truncateUtf8(std::string_view,std::size_t); }
