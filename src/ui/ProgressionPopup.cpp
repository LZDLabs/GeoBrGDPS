#include "ProgressionPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../services/GeoPassService.hpp"
#include "../models/JsonModelUtils.hpp"
#include <algorithm>
#include <Geode/ui/ScrollLayer.hpp>
#include <cmath>
using namespace geode::prelude;
namespace geobr {
namespace {
void text(CCNode* p,std::string const& s,CCPoint at,float scale,float width,ccColor3B color={255,255,255}){auto l=ui::makeLabel(s,"bigFont.fnt",scale,color);l->setPosition(at);l->limitLabelWidth(width,scale,.18f);p->addChild(l,3);}
matjson::Value field(matjson::Value const& v,char const* key){auto x=v.get(key);return x?x.unwrap():matjson::Value(nullptr);}
const char* rarityNames[]={"Comum","Incomum","Rara","Epica","Lendaria","Mitica"};
class BundleReveal final:public Popup {
 std::vector<matjson::Value> m_items;CCNode* m_view=nullptr;int m_index=0;bool m_summary=false;
 bool init(matjson::Value const& items,int rarity){
  if(!Popup::init(470,300))return false;visual::fitPopup(m_mainLayer,{470,300});visual::stylePopup(m_mainLayer,m_bgSprite,{470,300},{42,33,66});m_items=items.asArray().unwrap();if(m_items.empty())return false;render();return true;
 }
 void onNext(CCObject*){if(m_summary){onClose(nullptr);return;}if(++m_index>=m_items.size())m_summary=true;render();}
 void onAll(CCObject*){m_summary=true;render();}
 void render(){
  if(m_view)m_view->removeFromParent();m_view=CCNode::create();m_mainLayer->addChild(m_view);auto menu=CCMenu::create();menu->setPosition({0,0});m_view->addChild(menu,5);
  if(!m_summary){auto const& r=m_items[m_index];text(m_view,fmt::format("ITEM {} DE {}",m_index+1,m_items.size()),{235,267},.50f,370,{255,226,132});
   auto pedestal=ui::makePanel({204,139},{36,42,74},255);pedestal->setPosition({235,176});m_view->addChild(pedestal);auto icon=visual::rewardIcon(r,91);icon->setPosition({235,183});m_view->addChild(icon);auto scale=icon->getScale();icon->setScale(scale*.6f);icon->runAction(CCEaseBackOut::create(CCScaleTo::create(.24f,scale)));
   text(m_view,GeoPassService::rewardLabel(r),{235,101},.43f,410,{244,238,255});text(m_view,jsonutil::boolOr(r,"exclusive")?"EXCLUSIVO DE CAIXAS":jsonutil::boolOr(r,"duplicate")?"Repetido convertido em Tokens":"Adicionado ao inventario",{235,78},.25f,410,{148,238,209});
   auto all=ui::makeTextButton("Revelar tudo",154,"GJ_button_04.png",this,menu_selector(BundleReveal::onAll),.29f,29);all->setPosition({130,38});menu->addChild(all);
  }else{
   text(m_view,"VOCE RECEBEU!",{235,268},.54f,390,{255,226,132});for(int i=0;i<m_items.size()&&i<10;++i){float x=67.f+(i%5)*84,y=211.f-(i/5)*95;auto card=ui::makePanel({77,86},{36,42,74},255);card->setPosition({x,y});m_view->addChild(card);auto icon=visual::rewardIcon(m_items[i],39);icon->setPosition({x,y+11});m_view->addChild(icon);text(m_view,GeoPassService::rewardLabel(m_items[i]),{x,y-26},.22f,71);}
   text(m_view,"Abertura salva. Consulte o Historico no menu Opcoes.",{235,64},.23f,420,{175,219,230});
  }
  auto next=ui::makeTextButton(m_summary?"Concluir":m_index+1==m_items.size()?"Ver resumo":"Proximo",170,"GJ_button_01.png",this,menu_selector(BundleReveal::onNext),.33f,31);next->setPosition({m_summary?235.f:330.f,38});menu->addChild(next);
 }
public:static BundleReveal* create(matjson::Value const& items,int rarity){auto p=new BundleReveal;if(p->init(items,rarity)){p->autorelease();return p;}delete p;return nullptr;}
};
class RewardReveal final:public Popup {
 bool init(matjson::Value const& reward,int rarity){
  if(!Popup::init(330,250))return false;visual::fitPopup(m_mainLayer,{330,250});visual::stylePopup(m_mainLayer,m_bgSprite,{330,250},{53,39,78});auto bg=visual::theme({316,236},"cosmos");bg->setPosition({7,7});m_mainLayer->addChild(bg,-1);
  text(m_mainLayer,"RECOMPENSA!",{165,217},.62f,280,{255,231,128});
  auto pedestal=ui::makePanel({180,112},{62,40,102},255);pedestal->setPosition({165,137});m_mainLayer->addChild(pedestal);
  auto icon=visual::rewardIcon(reward,78);icon->setPosition({165,143});m_mainLayer->addChild(icon,3);
  auto targetScale=icon->getScale();icon->setScale(targetScale*.25f);
  if(rarity>0){
   auto box=visual::rewardIcon(matjson::makeObject({{"kind","crate"},{"crate_rarity",rarity}}),95);box->setPosition({165,143});m_mainLayer->addChild(box,4);
   box->runAction(CCSequence::create(CCRotateTo::create(.1f,-8),CCRotateTo::create(.1f,8),CCRotateTo::create(.1f,-5),CCRotateTo::create(.1f,0),CCFadeOut::create(.15f),nullptr));
   icon->setVisible(false);icon->runAction(CCSequence::create(CCDelayTime::create(.55f),CCShow::create(),CCEaseBackOut::create(CCScaleTo::create(.35f,targetScale)),nullptr));
  }else icon->runAction(CCEaseBackOut::create(CCScaleTo::create(.35f,targetScale)));
  text(m_mainLayer,GeoPassService::rewardLabel(reward),{165,65},.43f,290,{224,241,255});
  text(m_mainLayer,jsonutil::boolOr(reward,"duplicate")?"Repetido convertido em Tokens":"Adicionado ao seu inventario",{165,36},.25f,290,{176,221,206});return true;
 }
 public:static RewardReveal* create(matjson::Value const& r,int rarity=0){auto p=new RewardReveal;if(p->init(r,rarity)){p->autorelease();return p;}delete p;return nullptr;}
};}
void showCrateBundle(matjson::Value const& items,int rarity){if(items.isArray())if(auto p=BundleReveal::create(items,rarity))p->show();}
ProgressionPopup* ProgressionPopup::create(int tab){auto p=new ProgressionPopup;if(p->init(tab)){p->autorelease();return p;}delete p;return nullptr;}
bool ProgressionPopup::init(int tab){
 if(!Popup::init(550,340))return false;visual::fitPopup(m_mainLayer,{550,340});visual::stylePopup(m_mainLayer,m_bgSprite,{550,340},{53,39,78});m_tab=tab;setID("geobr-progression");auto win=CCDirector::sharedDirector()->getWinSize();
 auto bg=visual::theme({536,326},"cosmos");bg->setPosition({7,7});m_mainLayer->addChild(bg,-1);
 m_status=ui::makeLabel("","chatFont.fnt",.38f,{170,234,255});m_status->setPosition({275,12});m_mainLayer->addChild(m_status,5);
 m_state=GeoPassService::get()->state();render();onRefresh(nullptr);return true;
}
void ProgressionPopup::onTab(CCObject* s){if(m_busy)return;m_tab=s->getTag();render();if(m_tab==2){m_busy=true;m_request.call("GET","/rest/v1/geobr_profiles?select=display_name,lifetime_xp,icon_id,icon_slug,color1,color2&order=lifetime_xp.desc,display_name.asc&limit=100",{},[this](auto r){m_busy=false;if(r.success)m_leaderboard=r.value;else m_status->setString(r.error.c_str());render();});}}
void ProgressionPopup::onRefresh(CCObject*){if(m_busy)return;m_busy=true;m_request.rpc("geobr_state_v2",matjson::Value::object(),[this](auto r){finish(std::move(r));});}
void ProgressionPopup::finish(ApiJsonResult r){
 m_busy=false;if(!r.success){m_revealRarity=0;m_status->setString(r.error.c_str());m_status->limitLabelWidth(500,.38f,.2f);render();return;}
 auto state=field(r.value,"state");if(!state.isObject())state=r.value;GeoPassService::get()->ingest(state);m_state=GeoPassService::get()->state();m_status->setString("");render();GeoPassService::get()->syncRewards();
 auto items=field(r.value,"items");if(items.isArray()){Mod::get()->setSavedValue("last-crate-items",items);if(auto saved=Mod::get()->saveData();saved.isErr())log::warn("GeoBR: local reveal state could not be saved; server history is authoritative");if(auto popup=BundleReveal::create(items,std::max(1,m_revealRarity)))popup->show();m_revealRarity=0;return;}
 auto reward=field(r.value,"reward");if(reward.isObject())if(auto popup=RewardReveal::create(reward,m_revealRarity))popup->show();m_revealRarity=0;
}
void ProgressionPopup::render(){
 if(m_view)m_view->removeFromParent();m_view=CCNode::create();m_mainLayer->addChild(m_view);
 text(m_view,m_tab==2?"RANKING GEOBR":m_tab?"COFRE GEOBR":"SUA JORNADA",{275,309},.67f,440,{255,231,143});
 auto menu=CCMenu::create();menu->setPosition({0,0});m_view->addChild(menu,5);
 for(int i=0;i<3;++i){auto b=ui::makeTextButton(i==2?"Ranking":i?"Caixas":"Levels",145,m_tab==i?"GJ_button_01.png":"GJ_button_04.png",this,menu_selector(ProgressionPopup::onTab),.35f,27);b->setPosition({110.f+i*165,275});b->setTag(i);menu->addChild(b);}
 if(m_tab==2){
  auto list=ScrollLayer::create({492,218},true,true);list->setPosition({29,32});list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(5));m_view->addChild(list);
  if(m_leaderboard.isArray()){int rank=0;for(auto const& row:m_leaderboard.asArray().unwrap()){
   auto card=ui::makePanel({484,45},rank<3?ccColor3B{64,44,92}:ccColor3B{30,32,57},255);card->setAnchorPoint({.5f,.5f});
   text(card,fmt::format("#{}",++rank),{25,23},.35f,42,{255,221,128});
   auto avatar=visual::avatar(row,27);avatar->setPosition({65,22});card->addChild(avatar,2);
   text(card,jsonutil::stringOr(row,"display_name"),{220,24},.34f,252);
   auto xp=jsonutil::integerOr(row,"lifetime_xp");text(card,fmt::format("Nv. {} | {} XP",int(std::sqrt(double(xp)/100))+1,ui::compactNumber(xp)),{410,24},.25f,132,{172,225,249});
   list->m_contentLayer->addChild(card);
  }}
  list->m_contentLayer->updateLayout();list->scrollToTop();text(m_view,"Ranking por XP permanente de missoes",{275,20},.23f,470,{188,204,230});return;
 }
 if(!m_tab){
  auto p=field(m_state,"profile");auto lv=jsonutil::integerOr(p,"level",1),xp=jsonutil::integerOr(p,"lifetime_xp"),start=jsonutil::integerOr(p,"level_start_xp"),next=std::max(start+1,jsonutil::integerOr(p,"next_level_xp",100));
  auto hero=ui::makePanel({494,65},{40,36,83},255);hero->setPosition({275,220});m_view->addChild(hero);
  text(m_view,fmt::format("LEVEL {}",lv),{121,231},.63f,165,{255,233,143});
  text(m_view,fmt::format("{} / {} XP",xp-start,next-start),{366,231},.30f,245,{197,229,255});
  auto bar=visual::bar({454,10},float(xp-start)/(next-start),{173,109,255});bar->setPosition({48,201});m_view->addChild(bar);
  text(m_view,"Cada Level Up libera uma recompensa permanente",{275,175},.27f,490,{206,206,235});
  auto rewards=field(m_state,"level_rewards");if(rewards.isArray()){
   auto rows=rewards.asArray().unwrap();for(int i=0;i<rows.size()&&i<6;++i){float x=65.f+i*84;auto const& row=rows[i];auto r=field(row,"reward");bool ready=jsonutil::boolOr(row,"ready");
    auto card=ui::makePanel({77,103},ready?ccColor3B{56,74,105}:ccColor3B{33,30,58},255);card->setPosition({x,110});m_view->addChild(card);
    text(m_view,fmt::format("NV. {}",jsonutil::integerOr(row,"level")),{x,149},.28f,69,{255,230,148});
    auto icon=visual::rewardIcon(r,39);icon->setPosition({x,115});m_view->addChild(icon,2);
    text(m_view,GeoPassService::rewardLabel(r),{x,77},.23f,68,{213,223,245});
   }
   if(!rows.empty()){auto first=rows.front();bool ready=jsonutil::boolOr(first,"ready");auto b=ui::makeTextButton(ready?"Resgatar Level Up":"Ganhe XP nas missoes",246,ready?"GJ_button_01.png":"GJ_button_04.png",this,menu_selector(ProgressionPopup::onClaim),.32f,29);b->setTag(jsonutil::integerOr(first,"level"));b->setPosition({275,35});b->setEnabled(ready&&!m_busy);menu->addChild(b);}
  }
 }else{
  auto boxes=field(m_state,"crates");auto list=boxes.isArray()?boxes.asArray().unwrap():std::vector<matjson::Value>{};m_crates.assign(6,"");
  for(int i=0;i<6;++i){int count=0;for(auto const& c:list)if(jsonutil::integerOr(c,"rarity")==i+1){++count;if(m_crates[i].empty())m_crates[i]=jsonutil::stringOr(c,"id");}
   float x=110.f+(i%3)*165,y=204.f-(i/3)*103;
   auto bg=ui::makePanel({153,96},i>=4?ccColor3B{73,43,67}:ccColor3B{35,34,64},255);bg->setPosition({x,y});m_view->addChild(bg);
   text(m_view,fmt::format("{}  x{}",rarityNames[i],count),{x,y+36},.28f,140,{255,232,157});
   text(m_view,fmt::format("{} itens",(i==5?10:i+4)),{x+53,y+16},.19f,43,{193,223,246});
   auto icon=visual::rewardIcon(matjson::makeObject({{"kind","crate"},{"crate_rarity",i+1}}),53);icon->setPosition({x,y+1});m_view->addChild(icon,2);
   auto b=ui::makeTextButton(count?"Abrir":"Sem caixas",119,count?"GJ_button_01.png":"GJ_button_04.png",this,menu_selector(ProgressionPopup::onOpen),.27f,22);b->setTag(i);b->setPosition({x,y-33});b->setEnabled(count&&!m_busy);menu->addChild(b);
  }
  auto info=ui::makeTextButton("Chances e recompensas",215,"GJ_button_04.png",this,menu_selector(ProgressionPopup::onInfo),.28f,24);info->setPosition({275,31});menu->addChild(info);
 }
}
void ProgressionPopup::onClaim(CCObject* s){if(m_busy)return;int level=s->getTag();m_busy=true;render();m_request.rpc("geobr_progression",matjson::makeObject({{"p_action","level"},{"p_payload",matjson::makeObject({{"level",level}})}}),[this](auto r){finish(std::move(r));});}
void ProgressionPopup::onOpen(CCObject* s){int i=s->getTag();if(m_busy||i<0||i>=m_crates.size()||m_crates[i].empty())return;auto id=m_crates[i];m_revealRarity=i+1;m_busy=true;render();m_request.rpc("geobr_open_crate_v4",matjson::makeObject({{"p_id",id}}),[this](auto r){finish(std::move(r));});}
void ProgressionPopup::onInfo(CCObject*){FLAlertLayer::create(nullptr,"Caixas GeoBR",
 "<cy>Itens por caixa</c>\nComum 4 | Incomum 5 | Rara 6\nEpica 7 | Lendaria 8 | Mitica 10\n\nSempre Tokens e Orbs. Rara+: 1 skin.\nEpica+: 1 titulo ou tema exclusivo.\nMitica: 2 skins + 1 cosmetico exclusivo.\nRepetidos registrados: 100 Tokens.\n\n<cy>Cada slot extra aleatorio</c>\n35% Tokens | 30% Orbs | 15% Skips\n12% skin | 4% titulo | 4% tema exclusivo.\n\n<cy>Drop ao concluir no normal</c>\nEasy 10% | Normal 15% | Hard 20%\nHarder 25% | Insane 35% | Demon 45%\nMinimo 15s. 6 tentativas por dia em fases diferentes.\nRaridade: 60% / 25% / 10% / 4% / 0.9% / 0.1%.","OK",nullptr,390,true,250,.65f)->show();}
}
