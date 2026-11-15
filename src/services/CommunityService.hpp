#pragma once

#include "ServiceResult.hpp"
#include "../models/CommunityMessage.hpp"
#include "../models/PlayerIdentity.hpp"

#include <Geode/utils/web.hpp>
#include <string>
#include <vector>

namespace geobr {

class CommunityService final {
public:
    static geode::utils::web::WebFuture fetchMessages();
    static geode::utils::web::WebFuture sendMessage(
        PlayerIdentity const& player,
        std::string const& message,
        std::string const& accessToken,
        std::string const& userID
    );
    static ServiceResult<std::vector<CommunityMessage>> parseMessages(
        geode::utils::web::WebResponse const& response
    );
    static ServiceResult<CommunityMessage> parseSentMessage(
        geode::utils::web::WebResponse const& response
    );
};

}
