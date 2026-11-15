#pragma once
#include <cstdint>
#include <string>
namespace geobr {
struct PlayerIdentity final {
    std::string username;
    std::int64_t accountId = 0;
    std::int64_t playerId = 0;
    [[nodiscard]] bool isLoggedIn() const { return accountId > 0 && !username.empty(); }
};
}
