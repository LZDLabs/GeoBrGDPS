#include "AdminForm.hpp"
#include "UIHelpers.hpp"
#include "GeoBRVisuals.hpp"
#include "../services/RankService.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../models/JsonModelUtils.hpp"
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/GameManager.hpp>
#include <hjfod.gmd-api/include/GMD.hpp>
#include <charconv>
#include <ctime>
#include <random>
using namespace geode::prelude;
namespace geobr {
namespace {
std::string dateAfter(int days){auto t=std::time(nullptr)+days*86400;char s[32];std::strftime(s,sizeof(s),"%Y-%m-%d",std::gmtime(&t));return s;}
std::string scalar(matjson::Value const& row,std::string const& key,std::string fallback){auto r=row.get(key);if(!r)return fallback;auto v=r.unwrap();if(v.isString())return v.asString().unwrap();if(v.isNumber())return std::to_string(v.asInt().unwrapOr(0));if(v.isBool())return v.asBool().unwrapOr(false)?"true":"false";return fallback;}
}
std::vector<AdminForm::Field> AdminForm::fields()const{
    switch(m_entity){
        case AdminEntity::Ranks:return {{"level_id","ID online do nivel","int","1"},{"position","Posicao TOP","int","1"},{"published","Publicado","bool","true"}};
        case AdminEntity::Links:return {{"account_id","Conta GDPS verificada","int","1"},{"user_id","UUID GeoBR do jogador"}};
        case AdminEntity::News:return {{"title","Titulo"},{"description","Texto"},{"author","Autor","text","Equipe GeoBR"},{"type","Tipo","select","notice",{"notice","update","news","event","maintenance"}},{"pinned","Fixar","bool","false"},{"priority","Prioridade (0-100)","int","0"},{"published","Publicado","bool","false"}};
        case AdminEntity::Announcements:return {{"title","Titulo"},{"message","Mensagem (ate 200)"},{"expires_at","Expira em (UTC)","date",dateAfter(1)},{"published","Publicado","bool","true"}};
        case AdminEntity::Seasons:return {{"name","Nome"},{"starts_at","Inicio (UTC)","date",dateAfter(30)},{"ends_at","Fim (UTC)","date",dateAfter(60)},{"xp_per_tier","XP por etapa","int","100"},{"published","Publicado","bool","false"}};
        case AdminEntity::Rewards:return {{"season_id","ID da temporada","int","1"},{"tier","Etapa (1-100)","int","1"},{"kind","Premio","select","tokens",{"tokens","orbs","stars","skin","crate","quest_skip","theme","arrow"}},{"amount","Quantidade (skin=1)","int","25"},{"icon_type","Tipo da skin","select","cube",{"cube","ship","ball","ufo","wave","robot","spider","swing","jetpack"}},{"icon_id","ID da skin","int","1"},{"crate_rarity","Caixa: 1 Comum a 6 Mitica","int","1"},{"item_id","Item (tema/arrow), ex.: arrow-9","text",""}};
        case AdminEntity::Quests:return {{"season_id","ID da temporada","int","1"},{"sequence_order","Posicao na sequencia","int","65"},{"title","Titulo"},{"kind","Objetivo","select","complete",{"complete","death","reach"}},{"goal","Levels / tentativas","int","1"},{"difficulty","Dificuldade","select","any",{"any","auto","easy","normal","hard","harder","insane","demon"}},{"scope","Origem dos levels","select","any",{"any","main","online","official"}},{"mode","Modo de jogo","select","normal",{"any","normal","practice"}},{"platformer","Formato","select","any",{"any","classic","platformer"}},{"min_percent","Porcentagem minima","int","0"},{"max_percent","Porcentagem maxima","int","100"},{"xp","XP ao concluir","int","100"},{"tokens","Tokens ao concluir","int","8"},{"published","Publicado","bool","true"}};
        case AdminEntity::Shop:return {{"id","ID (slug unico)","text",""},{"name","Nome"},{"kind","Categoria","select","title",{"title","theme","tier_skip","quest_skip","arrow"}},{"price","Preco em Tokens","int","40"},{"style","Estilo de tema","select","",{"","fire","ice","forest","cosmos","neon","gold","aurora","eclipse","royal","astral"}},{"description","Descricao"},{"sort_order","Ordem na loja","int","50"},{"published","Publicado","bool","true"}};
        case AdminEntity::MainLevels:return {{"campaign_order","Ordem na campanha","int","1"},{"name","Nome"},{"creator","Criador","text","Equipe GeoBR"},{"description","Descricao"},{"difficulty","Dificuldade","select","Normal",{"Auto","Easy","Normal","Hard","Harder","Insane","Easy Demon","Medium Demon","Hard Demon","Insane Demon","Extreme Demon"}},{"stars","Estrelas (0-10)","int","3"},{"coins","Moedas (0-3)","int","3"},{"song","Nome da musica"},{"platformer","Plataforma","bool","false"},{"gmd_path","Arquivo no servidor"},{"file_version","Versao do arquivo","int","1"},{"published","Publicado","bool","false"}};
    }return {};
}
AdminForm* AdminForm::create(AdminEntity entity,matjson::Value row){auto p=new AdminForm;if(p->init(entity,std::move(row))){p->autorelease();return p;}delete p;return nullptr;}
bool AdminForm::init(AdminEntity entity,matjson::Value row){
    if(!Popup::init(500,300))return false;visual::fitPopup(m_mainLayer,{500,300});visual::stylePopup(m_mainLayer,m_bgSprite,{500,300},{53,39,78});auto bg=visual::theme({486,286},"cosmos");bg->setPosition({7,7});m_mainLayer->addChild(bg,-1);m_entity=entity;m_row=std::move(row);m_id=jsonutil::integerOr(m_row,"id");m_recordID=scalar(m_row,"id","");setTitle((!m_recordID.empty()?"Editar - ":"Criar - ")+adminTitle(entity),"goldFont.fnt",.58f,17);setID("geobr-admin-form");
    auto scroll=ScrollLayer::create({462,213},true,true);scroll->setPosition({19,49});scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(6));m_mainLayer->addChild(scroll);
    for(auto field:fields()){
        Input input;input.field=field;auto v=scalar(m_row,field.key,field.initial);
        auto node=CCNode::create();node->setContentSize({456,42});node->setAnchorPoint({.5f,.5f});
        auto label=ui::makeLabel(field.label,"chatFont.fnt",.52f,{216,236,255});label->setAnchorPoint({0,.5f});label->setPosition({4,21});label->limitLabelWidth(161,.52f,.3f);node->addChild(label);
        if(field.type=="bool"){
            auto menu=CCMenu::create();menu->setPosition({202,21});input.toggle=CCMenuItemToggler::createWithStandardSprites(this,nullptr,.6f);input.toggle->toggle(v=="true");menu->addChild(input.toggle);node->addChild(menu);
        }else if(field.type=="select"){
            for(int i=0;i<field.options.size();++i)if(field.options[i]==v)input.choice=i;
            auto spr=ButtonSprite::create(field.options[input.choice].c_str(),256,true,"bigFont.fnt","GJ_button_04.png",30,.34f);input.label=spr->m_label;spr->setScale(std::min(256.f/spr->getContentWidth(),30.f/spr->getContentHeight()));
            auto button=CCMenuItemSpriteExtra::create(spr,this,menu_selector(AdminForm::onChoice));button->setTag(m_inputs.size());auto menu=CCMenu::create();menu->setPosition({310,21});menu->addChild(button);node->addChild(menu);
        }else{
            input.text=TextInput::create(270,field.type=="date"?"AAAA-MM-DD":field.label);input.text->setCommonFilter(field.type=="int"?CommonFilter::Uint:CommonFilter::Any);input.text->setMaxCharCount(field.key=="description"?1000:300);input.text->setString(v);input.text->setPosition({312,21});if(field.key=="id"&&!m_recordID.empty())input.text->setEnabled(false);node->addChild(input.text);
        }
        m_inputs.push_back(input);scroll->m_contentLayer->addChild(node);
    }
    scroll->m_contentLayer->updateLayout();scroll->scrollToTop();
    auto menu=CCMenu::create();menu->setPosition({0,0});auto save=ui::makeTextButton("Salvar",122,"GJ_button_01.png",this,menu_selector(AdminForm::onSave),.38f,28);save->setPosition({entity==AdminEntity::MainLevels?360.f:250.f,21});menu->addChild(save);
    if(entity==AdminEntity::MainLevels){auto file=ui::makeTextButton("Escolher .gmd",182,"GJ_button_04.png",this,menu_selector(AdminForm::onFile),.35f,28);file->setPosition({128,21});menu->addChild(file);}
    m_mainLayer->addChild(menu,5);m_status=ui::makeLabel("","chatFont.fnt",.43f,{178,238,255});m_status->setPosition({250,43});m_mainLayer->addChild(m_status);return true;
}
void AdminForm::status(std::string const& text){m_status->setString(text.c_str());m_status->limitLabelWidth(461,.43f,.23f);}
void AdminForm::setValue(std::string const& key,std::string const& v){for(auto& input:m_inputs)if(input.field.key==key&&input.text)input.text->setString(v);}
std::string AdminForm::value(std::string const& key)const{for(auto const& input:m_inputs)if(input.field.key==key){if(input.text)return input.text->getString();if(input.toggle)return input.toggle->isToggled()?"true":"false";return input.field.options[input.choice];}return {};}
void AdminForm::onChoice(CCObject* sender){if(m_busy)return;auto& input=m_inputs.at(sender->getTag());input.choice=(input.choice+1)%input.field.options.size();input.label->setString(input.field.options[input.choice].c_str());input.label->limitLabelWidth(238,.34f,.22f);if(input.field.key=="kind"&&(input.field.options[input.choice]=="skin"||input.field.options[input.choice]=="crate"))setValue("amount","1");}
void AdminForm::onSave(CCObject*){
    if(m_busy)return;auto data=matjson::Value::object();
    for(auto const& input:m_inputs){auto const& f=input.field;auto v=value(f.key);
        if(f.type=="int"){int64_t n=0;auto parsed=std::from_chars(v.data(),v.data()+v.size(),n);if(parsed.ec!=std::errc{}||parsed.ptr!=v.data()+v.size()){status("Numero invalido: "+f.label);return;}data[f.key]=n;}
        else if(f.type=="bool")data[f.key]=v=="true";
        else if(f.key=="item_id"&&v.empty())data[f.key]=nullptr; // optional FK to a shop item
        else{if(f.type=="date"&&v.size()==10)v+="T00:00:00Z";data[f.key]=v;}
    }
    if(m_entity==AdminEntity::MainLevels&&!GeoBRConfig::validStoragePath(value("gmd_path"))){status("Escolha um arquivo .gmd antes de salvar.");return;}
    if(m_entity==AdminEntity::Rewards&&value("kind")=="skin"){
        auto name=value("icon_type");IconType type=IconType::Cube;
        if(name=="ship")type=IconType::Ship;else if(name=="ball")type=IconType::Ball;else if(name=="ufo")type=IconType::Ufo;
        else if(name=="wave")type=IconType::Wave;else if(name=="robot")type=IconType::Robot;else if(name=="spider")type=IconType::Spider;
        else if(name=="swing")type=IconType::Swing;else if(name=="jetpack")type=IconType::Jetpack;
        auto id=jsonutil::integerOr(data,"icon_id");auto count=GameManager::sharedState()->countForType(type);
        if(id<1||id>count){status(fmt::format("Skin invalida: escolha um ID entre 1 e {}.",count));return;}
        data["amount"]=1;
    }
    if(m_entity==AdminEntity::Ranks){
        m_busy=true;status("Salvando TOP...");m_save.rpc("geobr_save_rank_v3",matjson::makeObject({{"p_id",m_recordID.empty()?matjson::Value(nullptr):matjson::Value(jsonutil::integerOr(m_row,"id"))},{"p_level",data["level_id"]},{"p_position",data["position"]},{"p_published",data["published"]}}),[this](auto r){m_busy=false;if(!r.success){status(r.error);return;}m_row=r.value;m_recordID=std::to_string(jsonutil::integerOr(r.value,"id"));RankService::get()->refresh();status(fmt::format("TOP {} salvo para ID {}!",jsonutil::integerOr(r.value,"position"),jsonutil::integerOr(r.value,"level_id")));});return;
    }
    m_busy=true;status("Salvando para todos os jogadores...");
    m_save.call(!m_recordID.empty()?"PATCH":"POST","/rest/v1/"+adminTable(m_entity)+(!m_recordID.empty()?"?id=eq."+m_recordID:""),data,[this](auto result){
        m_busy=false;if(!result.success){status(result.error);return;}
        if(!result.value.isArray()||result.value.asArray().unwrap().empty()){status("Nao salvo. Reabra o ADM para renovar sua sessao.");return;}
        m_row=result.value.asArray().unwrap().front();m_id=jsonutil::integerOr(m_row,"id");m_recordID=scalar(m_row,"id","");status(fmt::format("Salvo! ID {}. Feche e atualize a lista.",m_recordID));
    });
}
void AdminForm::onFile(CCObject*){
    if(m_busy)return;m_busy=true;status("Escolha o arquivo do GDShare...");
    m_picker.spawn("GeoBR - escolher Main Level",file::pick(file::PickMode::OpenFile,{{},{{"GDShare",{"*.gmd","*.gmd2","*.lvl"}}}}),[this](auto result){
        if(result.isErr()||!result.unwrap()){m_busy=false;status(result.isErr()?"Nao foi possivel abrir o seletor.":"Selecao cancelada.");return;}
        auto path=*result.unwrap();std::error_code error;auto size=std::filesystem::file_size(path,error);
        if(error||size==0||size>GeoBRConfig::MaxGmdBytes){m_busy=false;status("O arquivo deve ter dados e no maximo 12 MB.");return;}
        auto imported=gmd::importGmdAsLevel(path);
        if(imported.isErr()||!imported.unwrap()||imported.unwrap()->m_levelString.empty()){m_busy=false;status("Arquivo GDShare invalido ou level vazio.");return;}
        auto data=file::readBinary(path);if(data.isErr()){m_busy=false;status("Falha ao ler o arquivo.");return;}
        auto extension=path.extension().string();if(extension!=".gmd"&&extension!=".gmd2"&&extension!=".lvl"){m_busy=false;status("Use .gmd, .gmd2 ou .lvl.");return;}
        auto name=fmt::format("main-levels/{}-{}{}",std::time(nullptr),std::random_device{}(),extension);
        std::string levelName=imported.unwrap()->m_levelName;
        status("Enviando arquivo...");
        m_upload.call("POST","/storage/v1/object/geobr-main-levels/"+name,{},[this,name,levelName](auto upload){
            m_busy=false;if(!upload.success){status(upload.error);return;}
            setValue("gmd_path",name);if(value("name").empty())setValue("name",levelName);
            setValue("file_version",std::to_string(m_id?jsonutil::integerOr(m_row,"file_version",1)+1:1));status("Arquivo enviado. Revise os campos e clique Salvar.");
        },true,std::move(data).unwrap());
    });
}
}
