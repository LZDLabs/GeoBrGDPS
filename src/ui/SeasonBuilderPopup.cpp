#include "SeasonBuilderPopup.hpp"
#include "UIHelpers.hpp"
#include "GeoBRVisuals.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/GeoPassService.hpp"
#include <charconv>
#include <algorithm>
using namespace geode::prelude;
namespace geobr {
SeasonBuilderPopup* SeasonBuilderPopup::create(){auto p=new SeasonBuilderPopup;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
bool SeasonBuilderPopup::init(){
 if(!Popup::init(520,330))return false;visual::fitPopup(m_mainLayer,{520,330});visual::stylePopup(m_mainLayer,m_bgSprite,{520,330},{53,39,78});setTitle("Criador de temporadas","goldFont.fnt",.65f,17);auto win=CCDirector::sharedDirector()->getWinSize();
 auto bg=visual::theme({506,316},"cosmos");bg->setPosition({7,7});m_mainLayer->addChild(bg,-1);
 m_name=TextInput::create(470,"Nome da proxima temporada","chatFont.fnt");m_name->setMaxCharCount(60);m_name->setString("Expedicao GeoBR");m_name->setPosition({260,270});m_mainLayer->addChild(m_name);
 const char* names[]={"Dias","Etapas","Missoes","XP / etapa"};const char* defaults[]={"30","30","24","100"};
 for(int i=0;i<4;++i){float x=75.f+123*i;auto l=ui::makeLabel(names[i],"bigFont.fnt",.28f,{206,225,255});l->setPosition({x,240});m_mainLayer->addChild(l);m_numbers[i]=TextInput::create(108,names[i],"chatFont.fnt");m_numbers[i]->setCommonFilter(CommonFilter::Uint);m_numbers[i]->setString(defaults[i]);m_numbers[i]->setPosition({x,215});m_mainLayer->addChild(m_numbers[i]);}
 m_preview=ScrollLayer::create({474,132},true,true);m_preview->setPosition({23,58});m_preview->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4));m_mainLayer->addChild(m_preview);
 auto menu=CCMenu::create();menu->setPosition({0,0});m_mainLayer->addChild(menu,5);
 auto generate=ui::makeTextButton("Gerar previa",194,"GJ_button_04.png",this,menu_selector(SeasonBuilderPopup::onGenerate),.35f,28);generate->setPosition({142,29});menu->addChild(generate);
 auto publish=ui::makeTextButton("Agendar passe",194,"GJ_button_01.png",this,menu_selector(SeasonBuilderPopup::onPublish),.35f,28);publish->setPosition({378,29});menu->addChild(publish);
 m_status=ui::makeLabel("A temporada atual sera preservada.","chatFont.fnt",.4f,{183,231,223});m_status->setPosition({260,51});m_mainLayer->addChild(m_status,5);return true;
}
void SeasonBuilderPopup::onGenerate(CCObject*){
 if(m_busy)return;auto cfg=matjson::makeObject({{"name",std::string(m_name->getString())}});const char* keys[]={"days","tiers","quests","xp_per_tier"};
 for(int i=0;i<4;++i){auto s=m_numbers[i]->getString();int n=0;auto r=std::from_chars(s.data(),s.data()+s.size(),n);if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()){m_status->setString("Preencha todos os numeros.");return;}cfg[keys[i]]=n;}
 m_busy=true;m_status->setString("Gerando previa...");m_request.rpc("geobr_season_builder",matjson::makeObject({{"p_action","generate"},{"p_payload",cfg}}),[this](auto r){m_busy=false;if(!r.success){m_status->setString(r.error.c_str());m_status->limitLabelWidth(470,.4f,.2f);return;}m_draft=r.value;render();});
}
void SeasonBuilderPopup::render(){
 ui::clearScroll(m_preview);auto c=m_draft.get("config");if(!c)return;auto cfg=c.unwrap();
 auto add=[this](std::string const& s){auto l=ui::makeLabel(s,"chatFont.fnt",.48f);l->limitLabelWidth(464,.48f,.25f);auto row=CCNode::create();row->setContentSize({464,20});row->setAnchorPoint({.5f,.5f});l->setPosition({232,10});row->addChild(l);m_preview->m_contentLayer->addChild(row);};
 add("Inicio UTC: "+jsonutil::stringOr(cfg,"starts_at").substr(0,19));add("Fim UTC: "+jsonutil::stringOr(cfg,"ends_at").substr(0,19));
 auto rewards=cfg.get("rewards");if(rewards&&rewards.unwrap().isArray())for(auto const& row:rewards.unwrap().asArray().unwrap())add(fmt::format("Etapa {}: {}",jsonutil::integerOr(row,"tier"),GeoPassService::rewardLabel(row)));
 auto quests=cfg.get("quests");if(quests&&quests.unwrap().isArray())for(auto const& row:quests.unwrap().asArray().unwrap())add(fmt::format("Missao {}: {} | +{} XP",jsonutil::integerOr(row,"sequence_order"),jsonutil::stringOr(row,"title"),jsonutil::integerOr(row,"xp")));
 m_preview->m_contentLayer->updateLayout();m_preview->scrollToTop();m_status->setString("Previa pronta. Confira antes de agendar.");
}
void SeasonBuilderPopup::onPublish(CCObject*){
 if(m_busy||jsonutil::stringOr(m_draft,"id").empty()){m_status->setString("Gere uma previa primeiro.");return;}
 WeakRef<SeasonBuilderPopup> weak=this;
 createQuickPopup("Agendar temporada?","A previa exibida sera publicada para comecar depois das temporadas ja agendadas. Alterar os campos exige gerar uma nova previa.","Cancelar","Agendar",[weak](auto,bool yes){auto self=weak.lock();if(!yes||!self||self->m_busy)return;self->m_busy=true;self->m_request.rpc("geobr_season_builder",matjson::makeObject({{"p_action","publish"},{"p_payload",matjson::makeObject({{"id",jsonutil::stringOr(self->m_draft,"id")}})}}),[weak](auto r){auto self=weak.lock();if(!self)return;self->m_busy=false;self->m_status->setString(r.success?"Temporada agendada com sucesso!":r.error.c_str());self->m_status->limitLabelWidth(470,.4f,.2f);});});
}
}
