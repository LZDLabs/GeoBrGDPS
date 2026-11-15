#include "CommunityService.hpp"

#include "ApiClient.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../models/JsonModelUtils.hpp"

namespace geobr {

geode::utils::web::WebFuture CommunityService::fetchMessages() {
    return ApiClient::getSupabase(GeoBRConfig::supabaseUrl()+"/rest/v1/rpc/geobr_chat_feed", {});
}

geode::utils::web::WebFuture CommunityService::sendMessage(
    PlayerIdentity const& player,
    std::string const& message,
    std::string const& accessToken,
    std::string const& userID
) {
    return ApiClient::postSupabaseAuthorized(
        GeoBRConfig::supabaseTableUrl(GeoBRConfig::CommunityTable), accessToken,
        matjson::makeObject({
            {"user_id", userID},
            {"username", player.username.empty() ? "Jogador GeoBR" : player.username},
            {"message", message},
            {"account_id", player.accountId},
            {"player_id", player.playerId}
        })
    );
}

ServiceResult<std::vector<CommunityMessage>> CommunityService::parseMessages(
    geode::utils::web::WebResponse const& response
) {
    auto decoded = ApiClient::decodeJson(response);
    if (!decoded.success) return ServiceResult<std::vector<CommunityMessage>>::fail(std::move(decoded.error));
    auto payload = jsonutil::arrayPayload(decoded.value, "messages");
    if (!payload) return ServiceResult<std::vector<CommunityMessage>>::fail("Formato do chat invalido.");
    auto array = payload->asArray();
    if (array.isErr()) return ServiceResult<std::vector<CommunityMessage>>::fail("Formato do chat invalido.");
    auto values = array.unwrap();
    std::vector<CommunityMessage> messages;
    for (auto const& value : values) {
        if (auto message = CommunityMessage::fromJson(value)) messages.push_back(std::move(*message));
    }
    if (!values.empty() && messages.empty()) {
        return ServiceResult<std::vector<CommunityMessage>>::fail("O Supabase nao enviou mensagens validas.");
    }
    return ServiceResult<std::vector<CommunityMessage>>::ok(std::move(messages));
}

ServiceResult<CommunityMessage> CommunityService::parseSentMessage(
    geode::utils::web::WebResponse const& response
) {
    auto decoded = ApiClient::decodeJson(response);
    if (!decoded.success) return ServiceResult<CommunityMessage>::fail(std::move(decoded.error));
    auto value = decoded.value;
    if (value.isArray()) {
        auto array = value.asArray();
        if (array.isOk() && !array.unwrap().empty()) value = array.unwrap().front();
    }
    if (auto message = CommunityMessage::fromJson(value)) {
        return ServiceResult<CommunityMessage>::ok(std::move(*message));
    }
    return ServiceResult<CommunityMessage>::fail("Confirmacao de envio invalida.");
}

}
