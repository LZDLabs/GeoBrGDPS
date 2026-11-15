#pragma once
#include <matjson.hpp>
#include <cstdint>
#include <optional>
#include <string>
namespace geobr { struct CommunityMessage final { matjson::Value avatar; std::string replyBody; std::string id,username,message,createdAt,userID,replyTo; std::int64_t accountId=0,playerId=0; static std::optional<CommunityMessage> fromJson(matjson::Value const&); }; }
