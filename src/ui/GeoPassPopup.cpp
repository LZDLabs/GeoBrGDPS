#include "GeoPassPopup.hpp"
#include "TierShopPopup.hpp"
#include "ProgressionPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../services/GeoPassService.hpp"
#include "../models/JsonModelUtils.hpp"
#include <algorithm>
using namespace geode::prelude;
namespace geobr {
namespace {
CCLabelBMFont* label(CCNode* p,std::string const& s,CCPoint at,float scale=.4f,float width=500,ccColor3B color={255,255,255},bool left=false){auto l=ui::makeLabel(s,"bigFont.fnt",scale,color);l->setAnchorPoint({left?0.f:.5f,.5f});l->setPosition(at);l->limitLabelWidth(width,scale,.2f);p->addChild(l);return l;}
matjson::Value object(matjson::Value const& s,char const* key){auto v=s.get(key);return v?v.unwrap():matjson::Value(nullptr);}
class QuestPopup final:public Popup {
 CCNode* m_view=nullptr;int64_t m_revision=-1;std::string m_syncStatus;
 void poll(float){auto rev=jsonutil::integerOr(GeoPassService::get()->state(),"revision");auto status=GeoPassService::get()->questStatus();if(rev!=m_revision||status!=m_syncStatus){m_revision=rev;m_syncStatus=status;render();}}
 CloudRequest m_skip;bool m_busy=false;
 void onShop(CCObject*){if(auto p=TierShopPopup::create(2))p->show();}
 void onSkip(CCObject*){if(m_busy)return;m_busy=true;m_skip.rpc("geobr_progression",matjson::makeObject({{"p_action","skip"},{"p_payload",matjson::makeObject({{"run",GeoPassService::get()->currentRun()}})}}),[this](auto r){m_busy=false;if(r.success){auto s=r.value.get("state");if(s)GeoPassService::get()->ingest(s.unwrap());render();}else FLAlertLayer::create("Nao foi possivel",r.error,"OK")->show();});}
 void render(){
  if(m_view)m_view->removeFromParent();m_view=CCNode::create();m_mainLayer->addChild(m_view);
  auto const& s=GeoPassService::get()->state();auto q=object(s,"quest");if(!q.isObject()){label(m_view,"Nenhuma quest ativa",{215,128});return;}
  auto progress=jsonutil::integerOr(q,"progress"),goal=std::max<int64_t>(1,jsonutil::integerOr(q,"goal"));
  label(m_view,fmt::format("QUEST {} / {}",jsonutil::integerOr(q,"sequence_order"),jsonutil::integerOr(s,"quest_count")),{215,199},.35f,380,{139,238,255});
  label(m_view,jsonutil::stringOr(q,"title"),{215,170},.49f,376,{255,230,115});
  auto mode=jsonutil::stringOr(q,"mode"),scope=jsonutil::stringOr(q,"scope");
  label(m_view,fmt::format("{}  |  {}",mode=="normal"?"Modo normal":mode=="practice"?"Modo pratica":"Normal ou pratica",scope=="main"?"Main Levels GeoBR":scope=="online"?"Levels online":scope=="official"?"Levels oficiais do GD":"GD e Main Levels GeoBR"),{215,145},.30f,370,{197,213,239});
  label(m_view,fmt::format("{} / {}",progress,goal),{215,119},.45f,250);
  auto bar=visual::bar({366,12},float(progress)/goal);bar->setPosition({32,95});m_view->addChild(bar);
  label(m_view,fmt::format("+{} XP  +{} Tokens",jsonutil::integerOr(q,"xp"),jsonutil::integerOr(q,"tokens")),{215,76},.37f,355,{255,224,102});
  label(m_view,GeoPassService::get()->questStatus(),{215,55},.26f,365,{188,227,242});
  auto menu=CCMenu::create();menu->setPosition({215,27});m_view->addChild(menu);auto shop=ui::makeTextButton("Tier Shop",142,"GJ_button_04.png",this,menu_selector(QuestPopup::onShop),.31f,25);shop->setPosition({-90,0});menu->addChild(shop);
  auto skips=jsonutil::integerOr(s,"skips");auto skip=ui::makeTextButton(fmt::format("Usar Skip ({})",skips),165,"GJ_button_01.png",this,menu_selector(QuestPopup::onSkip),.29f,25);skip->setPosition({90,0});skip->setEnabled(skips>0&&!m_busy);menu->addChild(skip);
 }
 bool init()override{if(!Popup::init(430,245))return false;visual::fitPopup(m_mainLayer,{430,245});visual::stylePopup(m_mainLayer,m_bgSprite,{430,245},{53,39,78});setTitle("Quest Pass","goldFont.fnt",.75f,14);render();schedule(schedule_selector(QuestPopup::poll),.25f);return true;}
public:static QuestPopup* create(){auto p=new QuestPopup;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
};}
GeoPassPopup* GeoPassPopup::create(){auto p=new GeoPassPopup;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
bool GeoPassPopup::init(){
 if(!Popup::init(550,340))return false;visual::fitPopup(m_mainLayer,{550,340});visual::stylePopup(m_mainLayer,m_bgSprite,{550,340},{53,39,78});setID("geobr-geopass");auto bg=visual::theme({536,326},"cosmos");bg->setPosition({7,7});m_mainLayer->addChild(bg,-1);
 m_status=ui::makeLabel("Carregando GeoPass...","chatFont.fnt",.38f,{170,234,255});m_status->setPosition({275,7});m_mainLayer->addChild(m_status,5);
 m_state=GeoPassService::get()->state();render();refresh();schedule(schedule_selector(GeoPassPopup::poll),.25f);return true;
}
void GeoPassPopup::poll(float){if(m_busy)return;auto const& s=GeoPassService::get()->state();auto rev=jsonutil::integerOr(s,"revision");if(rev!=m_revision){m_revision=rev;m_state=s;render();}}
void GeoPassPopup::refresh(){m_busy=true;m_request.rpc("geobr_state_v2",matjson::Value::object(),[this](auto r){finish(std::move(r));});}
void GeoPassPopup::finish(ApiJsonResult r){
 m_busy=false;if(!r.success){m_status->setString(r.error.c_str());m_status->limitLabelWidth(496,.38f,.23f);render();return;}
 GeoPassService::get()->ingest(r.value);m_state=GeoPassService::get()->state();m_revision=jsonutil::integerOr(m_state,"revision");m_status->setString("");render();GeoPassService::get()->syncRewards();
}
void GeoPassPopup::render(){
 if(m_view)m_view->removeFromParent();m_view=CCNode::create();m_mainLayer->addChild(m_view);
 auto hero=ui::makePanel({506,59},{49,36,90},255);hero->setPosition({275,291});m_view->addChild(hero);
 label(m_view,"GeoPass",{38,304},.72f,295,{255,229,123},true);
 auto season=object(m_state,"season"),profile=object(m_state,"profile");
 label(m_view,jsonutil::stringOr(season,"name","Expedicao GeoBR"),{38,279},.30f,317,{216,204,255},true);
 if(auto coin=CCSprite::create("geobr-token.png"_spr)){coin->setScale(24.f/coin->getContentWidth());coin->setPosition({424,302});m_view->addChild(coin);}
 label(m_view,ui::compactNumber(jsonutil::integerOr(m_state,"tokens")),{485,302},.43f,79,{255,233,141});
 label(m_view,"Nv. "+std::to_string(jsonutil::integerOr(profile,"level",1)),{469,279},.27f,100,{170,231,255});
 if(!season.isObject()){label(m_view,"Aguardando temporada...",{275,165},.47f,456);return;}
 auto xp=jsonutil::integerOr(m_state,"xp"),per=std::max<int64_t>(1,jsonutil::integerOr(season,"xp_per_tier",100));
 std::vector<matjson::Value> rewards;auto list=m_state.get("rewards");if(list&&list.unwrap().isArray())rewards=list.unwrap().asArray().unwrap();
 std::ranges::sort(rewards,[](auto const& a,auto const& b){return jsonutil::integerOr(a,"tier")<jsonutil::integerOr(b,"tier");});
 int maxTier=0,skins=0;for(auto const& r:rewards){maxTier=std::max(maxTier,int(jsonutil::integerOr(r,"tier")));if(jsonutil::stringOr(r,"kind")=="skin")++skins;}
 auto tier=std::min<int64_t>(xp/per,maxTier);bool done=maxTier>0&&tier>=maxTier;
 label(m_view,fmt::format("{} skins | {} recompensas | Ate {}",skins,maxTier,jsonutil::stringOr(season,"ends_at").substr(0,10)),{275,253},.24f,480,{198,206,235});
 auto progressPanel=ui::makePanel({506,39},{23,26,53},255);progressPanel->setPosition({275,227});m_view->addChild(progressPanel);
 label(m_view,done?"EXPEDICAO COMPLETA!":fmt::format("ETAPA {} / {}",tier,maxTier),{38,237},.30f,200,{255,235,151},true);
 label(m_view,done?fmt::format("{} XP",xp):fmt::format("{} XP para a proxima",(tier+1)*per-xp),{390,237},.24f,245,{213,225,255});
 auto bar=visual::bar({474,8},done?1.f:float(xp%per)/per,{179,111,255});bar->setPosition({38,213});m_view->addChild(bar);
 int pages=std::max(1,int((rewards.size()+3)/4));if(!m_positioned&&!rewards.empty()){m_page=std::min<int>(tier/4,pages-1);m_positioned=true;}m_page=std::clamp(m_page,0,pages-1);
 auto menu=CCMenu::create();menu->setPosition({0,0});m_view->addChild(menu,4);
 for(int side=0;side<2;++side){auto spr=CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");spr->setScale(.36f);spr->setFlipX(side==1);auto b=CCMenuItemSpriteExtra::create(spr,this,menu_selector(GeoPassPopup::onPage));b->setTag(side?1:-1);b->setPosition({side?532.f:18.f,145});b->setEnabled(side?m_page+1<pages:m_page>0);b->setVisible(pages>1);spr->setOpacity(b->isEnabled()?255:85);menu->addChild(b);}
 for(int i=0;i<4;++i){int index=m_page*4+i;if(index>=rewards.size())break;auto const& r=rewards[index];int step=jsonutil::integerOr(r,"tier");bool claimed=jsonutil::boolOr(r,"claimed"),ready=xp>=step*per;float x=95.f+i*120;
 auto kind=jsonutil::stringOr(r,"kind");bool skin=kind=="skin";
 auto card=ui::makePanel({108,109},claimed?ccColor3B{26,90,77}:ready?ccColor3B{79,44,130}:skin?ccColor3B{57,43,91}:ccColor3B{32,36,65},255);card->setPosition({x,147});m_view->addChild(card);
 label(m_view,fmt::format("ETAPA {}",step),{x,190},.25f,94,skin?ccColor3B{255,230,128}:ccColor3B{186,204,240});
 auto icon=visual::rewardIcon(r,43);icon->setPosition({x,155});m_view->addChild(icon,2);
 std::string caption=GeoPassService::rewardLabel(r);if(jsonutil::integerOr(r,"bonus_crate")>0)caption+=" + Caixa";
 label(m_view,caption,{x,124},.25f,96,{236,234,255});
 auto b=ui::makeTextButton(claimed?"Recebido":ready?"Resgatar":"Bloqueado",94,ready&&!claimed?"GJ_button_01.png":"GJ_button_04.png",this,menu_selector(GeoPassPopup::onClaim),.26f,25);b->setTag(jsonutil::integerOr(r,"id"));b->setPosition({x,99});b->setEnabled(ready&&!claimed&&!m_busy);menu->addChild(b);
 }
 for(int i=0;i<2;++i){auto b=ui::makeTextButton(i?"Caixas":"Levels",84,"GJ_button_04.png",this,menu_selector(GeoPassPopup::onProgress),.26f,22);b->setTag(i);b->setPosition({i?477.f:73.f,77});menu->addChild(b);}
 label(m_view,fmt::format("{} / {}",m_page+1,pages),{275,77},.22f,100,{174,170,209});
 auto q=object(m_state,"quest");auto panel=ui::makePanel({506,58},{28,29,56},255);panel->setPosition({275,42});m_view->addChild(panel);
 label(m_view,q.isObject()?jsonutil::stringOr(q,"title"):"Carregando missao...",{35,60},.29f,358,{255,235,145},true);
 auto goal=std::max<int64_t>(1,jsonutil::integerOr(q,"goal")),progress=jsonutil::integerOr(q,"progress");
 label(m_view,fmt::format("{}/{} | +{} XP | +{} Tokens",progress,goal,jsonutil::integerOr(q,"xp"),jsonutil::integerOr(q,"tokens")),{35,42},.24f,354,{180,213,251},true);
 auto qb=visual::bar({355,5},float(progress)/goal,{117,213,181});qb->setPosition({35,23});m_view->addChild(qb);
 auto quest=ui::makeTextButton("Missao",103,"GJ_button_01.png",this,menu_selector(GeoPassPopup::onQuest),.32f,28);quest->setPosition({459,40});menu->addChild(quest);
}
void GeoPassPopup::onProgress(CCObject* s){if(auto p=ProgressionPopup::create(s->getTag()))p->show();}
void GeoPassPopup::onPage(CCObject* sender){m_page+=sender->getTag();render();}
void GeoPassPopup::onQuest(CCObject*){if(auto p=QuestPopup::create())p->show();}
void GeoPassPopup::onClaim(CCObject* sender){if(m_busy)return;int id=sender->getTag();m_busy=true;m_status->setString("Resgatando...");render();m_request.rpc("geobr_reward_v2",matjson::makeObject({{"p_reward_id",id}}),[this](auto r){finish(std::move(r));});}
}
