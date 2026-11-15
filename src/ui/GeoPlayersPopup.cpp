#include "GeoPlayersPopup.hpp"
#include "DirectMessagesPopup.hpp"
#include "SocialPopups.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../models/JsonModelUtils.hpp"
#include <algorithm>
#include <cmath>
using namespace geode::prelude;
namespace geobr {
namespace {
void label(CCNode* p,std::string const& s,CCPoint pos,float scale,float width,ccColor3B color={255,255,255},bool left=false){auto l=ui::makeLabel(s,"bigFont.fnt",scale,color);l->setAnchorPoint({left?0.f:.5f,.5f});l->setPosition(pos);l->limitLabelWidth(width,scale,.23f);p->addChild(l);}
}
GeoPlayersPopup* GeoPlayersPopup::create(){auto p=new GeoPlayersPopup;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
bool GeoPlayersPopup::init(){
 if(!Popup::init(550,325))return false;visual::fitPopup(m_mainLayer,{550,325});visual::stylePopup(m_mainLayer,m_bgSprite,{550,325},{53,39,78});setID("geobr-player-directory");auto bg=visual::theme({534,309},"cosmos");bg->setPosition({8,8});m_mainLayer->addChild(bg,-1);
 label(m_mainLayer,"Jogadores GeoBR",{275,300},.65f,430,{255,230,120});
 m_input=TextInput::create(363,"Buscar pelo nome","chatFont.fnt");m_input->setMaxCharCount(40);m_input->setCommonFilter(CommonFilter::Name);m_input->setPosition({210,261});m_mainLayer->addChild(m_input,4);
 auto menu=CCMenu::create();menu->setPosition({0,0});m_mainLayer->addChild(menu,4);
 auto search=ui::makeTextButton("Buscar",105,"GJ_button_01.png",this,menu_selector(GeoPlayersPopup::onSearch),.38f,32);search->setPosition({462,261});menu->addChild(search);
 m_status=ui::makeLabel("","chatFont.fnt",.42f,{180,222,248});m_status->setPosition({275,15});m_mainLayer->addChild(m_status,4);
 fetch();return true;
}
void GeoPlayersPopup::onSearch(CCObject*){m_page=0;m_input->defocus();fetch();}
void GeoPlayersPopup::onPage(CCObject* s){if(m_busy)return;m_page=std::max(0,m_page+s->getTag());fetch();}
void GeoPlayersPopup::fetch(){
 m_busy=true;m_status->setString("Carregando jogadores...");render();
 m_request.rpc("geobr_players",matjson::makeObject({{"p_query",std::string(m_input->getString().c_str())},{"p_offset",m_page*3}}),[this](auto r){
  m_busy=false;if(r.success){m_data=r.value;m_status->setString("Toque em um jogador para ver o perfil, mandar DM ou chamar para um grupo.");m_status->limitLabelWidth(496,.42f,.23f);}
  else {m_status->setString(r.error.c_str());m_status->limitLabelWidth(496,.42f,.23f);}render();
 });
}
void GeoPlayersPopup::onProfile(CCObject* sender){
 if(m_busy)return;auto id=static_cast<CCNode*>(sender)->getID();auto rows=m_data.get("players");
 if(rows&&rows.unwrap().isArray())for(auto const& p:rows.unwrap().asArray().unwrap())if(jsonutil::stringOr(p,"user_id")==id){if(auto card=PlayerCardPopup::create(p))card->show();return;}
}
void GeoPlayersPopup::onGroups(CCObject*){if(auto p=GroupsPopup::create())p->show();}
void GeoPlayersPopup::render(){
 if(m_view)m_view->removeFromParent();m_view=CCNode::create();m_mainLayer->addChild(m_view);
 auto menu=CCMenu::create();menu->setPosition({0,0});m_view->addChild(menu);
 auto rows=m_data.get("players");int count=0;
 if(rows&&rows.unwrap().isArray())for(auto const& p:rows.unwrap().asArray().unwrap()){
  if(count>=3)break;auto style=jsonutil::stringOr(p,"theme_slug","default");auto card=visual::theme({498,57},style);
  auto shade=CCLayerColor::create({5,9,23,150},420,57);shade->setPosition({65,0});card->addChild(shade);
  auto icon=visual::avatar(p,33);icon->setPosition({33,29});card->addChild(icon,2);
  label(card,jsonutil::stringOr(p,"display_name","Jogador GeoBR"),{68,38},.44f,326,{255,255,255},true);
  label(card,jsonutil::stringOr(p,"title_name","Iniciante"),{68,16},.3f,326,visual::palette(style).accent,true);
  auto xp=std::max<int64_t>(0,jsonutil::integerOr(p,"lifetime_xp"));int level=std::floor(std::sqrt(double(xp)/100))+1;
  label(card,fmt::format("Nv. {}",level),{448,31},.35f,76,{255,231,118});
  auto button=CCMenuItemSpriteExtra::create(card,this,menu_selector(GeoPlayersPopup::onProfile));button->setID(jsonutil::stringOr(p,"user_id"));button->setPosition({275,215.f-count*65});button->setEnabled(!m_busy);menu->addChild(button);++count;
 }
 if(!count)label(m_view,m_busy?"Buscando...":"Nenhum jogador encontrado",{275,161},.44f,468,{204,225,247});
 auto total=jsonutil::integerOr(m_data,"total");int pages=std::max<int64_t>(1,(total+2)/3);
 label(m_view,fmt::format("{} / {}   |   {} jogador{}",m_page+1,pages,total,total==1?"":"es"),{275,39},.3f,304,{197,220,239});
 for(int side=0;side<2;++side){auto b=ui::makeTextButton(side?">":"<",63,"GJ_button_04.png",this,menu_selector(GeoPlayersPopup::onPage),.37f,24);b->setTag(side?1:-1);b->setPosition({side?482.f:68.f,39});b->setEnabled(!m_busy&&(side?m_page+1<pages:m_page>0));b->setVisible(pages>1);menu->addChild(b);}
}
}
