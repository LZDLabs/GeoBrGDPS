#include "../src/models/CommunityMessage.hpp"
#include "../src/models/JsonModelUtils.hpp"
#include "../src/models/MainLevel.hpp"
#include "../src/models/NewsItem.hpp"

#include <cassert>
#include <iostream>

namespace {
matjson::Value parse(char const* source) {
    auto result = matjson::parse(source);
    assert(result.isOk());
    return std::move(result).unwrap();
}
}

int main() {
    auto news = geobr::NewsItem::fromJson(parse(
        R"({"title":"Manutencao","description":"Teste","isNew":true,"priority":99,"pinned":true,"type":"Manutencao"})"
    ));
    assert(news && news->pinned && news->type == geobr::NewsType::Maintenance);
    assert(!geobr::NewsItem::fromJson(parse(R"({"title":"","description":"x"})")));

    auto message = geobr::CommunityMessage::fromJson(parse(
        R"({"username":"Player","message":"Ola","accountId":10,"playerId":20})"
    ));
    assert(message && message->accountId == 10);
    assert(!geobr::CommunityMessage::fromJson(parse(R"({"username":"Player","message":""})")));

    auto level = geobr::MainLevel::fromJson(parse(
        R"({"id":100,"campaign_order":2,"name":"Brasil Infinito","difficulty":"Harder","stars":7,"coins":3,"platformer":true,"gmd_path":"main-levels/brasil.gmd","file_version":4,"published":true})"
    ));
    assert(level && level->order == 2 && level->stars == 7 && level->published);
    assert(level->platformer && level->fileVersion == 4 && level->difficultyCode() == 4);
    assert(level->coins == 3);
    assert(level->modeLabel() == "PLATAFORMA");
    auto clamped = geobr::MainLevel::fromJson(parse(
        R"({"id":101,"name":"Teste","stars":99,"coins":99,"gmd_path":"teste.gmd"})"
    ));
    assert(clamped && clamped->stars == 10 && clamped->coins == 3 && clamped->fileVersion == 1);
    assert(!geobr::MainLevel::fromJson(parse(R"({"id":0,"name":"Invalido","gmd_path":"x.gmd"})")));
    assert(!geobr::MainLevel::fromJson(parse(R"({"id":102,"name":"Sem arquivo"})")));

    auto liveMessage = geobr::CommunityMessage::fromJson(parse(
        R"({"id":42,"username":"Luiz","message":"Ola","user_id":"auth-uid","created_at":"2026-09-08T12:00:00Z"})"
    ));
    assert(liveMessage && liveMessage->id == "42");
    auto liveNews = geobr::NewsItem::fromJson(parse(
        R"({"id":7,"title":"Temporada","description":"GeoPass","created_at":"2026-09-08T12:00:00Z"})"
    ));
    assert(liveNews && liveNews->id == "7" && liveNews->date == "2026-09-08T12:00:00Z");

    auto data = geobr::jsonutil::arrayPayload(parse(R"({"data":[]})"), "levels");
    assert(data && data->asArray().unwrap().empty());
    auto levels = geobr::jsonutil::arrayPayload(parse(R"({"levels":[]})"), "levels");
    assert(levels && levels->asArray().unwrap().empty());
    assert(!geobr::jsonutil::arrayPayload(parse(R"({"data":{}})"), "levels"));
    assert(matjson::parse("{resposta invalida}").isErr());
    assert(geobr::MainLevel::fromJson(parse(
        R"({"id":103,"name":"Padrao","difficulty":"Extreme Demon","gmd_path":"main-levels/final.gmd2"})"
    ))->difficultyCode() == 10);

    std::cout << "Models: 20 validacoes aprovadas\n";
}
