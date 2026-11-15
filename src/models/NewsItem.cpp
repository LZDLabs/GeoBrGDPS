#include "NewsItem.hpp"
#include "JsonModelUtils.hpp"
#include <algorithm>
#include <cctype>
namespace geobr {
namespace { std::string lower(std::string v) { std::ranges::transform(v, v.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));}); return v; } }
std::optional<NewsItem> NewsItem::fromJson(matjson::Value const& value) {
    if (!value.isObject()) return std::nullopt;
    auto title = jsonutil::stringField(value,"title"), desc = jsonutil::stringField(value,"description");
    if (!title || title->empty() || !desc) return std::nullopt;
    NewsItem item; item.id=jsonutil::stringOr(value,"id",std::to_string(jsonutil::integerOr(value,"id"))); item.title=std::move(*title); item.description=std::move(*desc);
    item.date=jsonutil::stringOr(value,"date",jsonutil::stringOr(value,"createdAt",jsonutil::stringOr(value,"created_at","Sem data")));
    item.author=jsonutil::stringOr(value,"author","Equipe GeoBR"); item.isNew=jsonutil::boolOr(value,"isNew",jsonutil::boolOr(value,"new"));
    item.priority=static_cast<int>(std::clamp<std::int64_t>(jsonutil::integerOr(value,"priority"),0,100)); item.pinned=jsonutil::boolOr(value,"pinned");
    auto type=lower(jsonutil::stringOr(value,"type","noticia"));
    if(type=="aviso"||type=="notice") item.type=NewsType::Notice; else if(type=="evento"||type=="event") item.type=NewsType::Event;
    else if(type=="manutencao"||type=="manutenção"||type=="maintenance") item.type=NewsType::Maintenance;
    else if(type=="atualizacao"||type=="atualização"||type=="update") item.type=NewsType::Update;
    return item;
}
std::string NewsItem::typeLabel() const { switch(type){case NewsType::Notice:return "Aviso";case NewsType::Event:return "Evento";case NewsType::Maintenance:return "Manutencao";case NewsType::Update:return "Atualizacao";default:return "Noticia";} }
}
