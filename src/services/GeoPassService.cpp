#include <Geode/binding/GameLevelManager.hpp>
#include "GeoPassService.hpp"
#include "MainLevelProgress.hpp"
#include "PlayerService.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../arrow/ArrowSkins.hpp"
#include "../config/GeoBRConfig.hpp"
#include <Geode/binding/GameStatsManager.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextArea.hpp>
#include <algorithm>
#include <ctime>
#define MORE_ICONS_EVENTS
#include <hiimjustin000.more_icons/include/MoreIcons.hpp>
using namespace geode::prelude;
namespace geobr {
namespace { void persistPassState(){if(auto result=Mod::get()->saveData();result.isErr())log::warn("GeoPass: local state could not be saved");} }

GeoPassService* GeoPassService::get(){static auto service=[](){auto p=new GeoPassService;p->init();return p;}();return service;}
void GeoPassService::start(){
 if(m_started)return;m_started=true;
 m_state=Mod::get()->getSavedValue<matjson::Value>("geobr-v2-state",matjson::Value::object());
 auto p=Mod::get()->getSavedValue<matjson::Value>("activity-pending-v2",matjson::Value::array());if(p.isArray())m_queue=p.asArray().unwrap();
 CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(GeoPassService::tick),this,10.f,false);CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(GeoPassService::drain),this,1.f,false);tick(0);
}
// Mission "follow the Elder Moderators": GD keeps the follows, so the mod
// checks them and the server completes the mission when all are followed.
static void checkFollowElders(matjson::Value const& state){
 auto q=state.get("quest");if(!q||jsonutil::stringOr(q.unwrap(),"kind")!="follow_elders")return;
 static CloudRequest elders,report;
 elders.rpc("geobr_elders",matjson::Value::object(),[](auto r){
  if(!r.success||!r.value.isArray())return;auto followed=matjson::Value::array();auto glm=GameLevelManager::sharedState();
  for(auto const& e:r.value.asArray().unwrap()){auto id=static_cast<int>(jsonutil::integerOr(e,"account_id"));if(id>0&&glm&&glm->isFollowingUser(id))followed.push(id);}
  report.rpc("geobr_quest_follow_elders",matjson::makeObject({{"p_followed",followed}}),[](auto f){if(f.success&&f.value.contains("state"))GeoPassService::get()->ingest(f.value["state"]);});
 });
}
void GeoPassService::tick(float){sendNext();if(!PlayLayer::get()){++m_ticks;if(m_ticks%3==1||!m_queue.empty())refresh();if(m_ticks%3==2)checkFollowElders(m_state);if(m_ticks%6==1)notices();
 auto cursor=Mod::get()->getSavedValue<int64_t>("social-cursor",0);
 if(cursor<=0){
  // First time on this device/account: start after the newest one, old ones are not shown again.
  m_social.call("GET","/rest/v1/geobr_social_notifications?select=id&order=id.desc&limit=1",{},[this](auto r){
   if(!r.success||!r.value.isArray())return;int64_t latest=1;
   for(auto const& n:r.value.asArray().unwrap())latest=std::max(latest,jsonutil::integerOr(n,"id"));
   Mod::get()->setSavedValue("social-cursor",latest);persistPassState();
  });
  return;
 }
 // Only recent ones (last 30 min) pop up; older ones (offline for a while) stay in the lists.
 auto since=std::time(nullptr)-1800;char stamp[32];std::strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%SZ",std::gmtime(&since));
 m_social.call("GET",fmt::format("/rest/v1/geobr_social_notifications?select=id,kind,body&order=id.asc&id=gt.{}&created_at=gt.{}&limit=5",cursor,stamp),{},[this,cursor](auto r){
 if(PlayLayer::get()||!r.success||!r.value.isArray())return;auto latest=cursor;
 for(auto const& n:r.value.asArray().unwrap()){Notification::create(jsonutil::stringOr(n,"body"),NotificationIcon::Info,4.f)->show();latest=std::max(latest,jsonutil::integerOr(n,"id"));}
 if(latest!=cursor){Mod::get()->setSavedValue("social-cursor",latest);persistPassState();}
 });}}
std::string GeoPassService::currentRun()const{auto q=m_state.get("quest");return q?jsonutil::stringOr(q.unwrap(),"run_id"):"";}
void GeoPassService::ingest(matjson::Value const& next){
 if(!next.isObject()||!next.contains("profile"))return;
 if(jsonutil::integerOr(next,"revision")<jsonutil::integerOr(m_state,"revision"))return;
 auto oldProfile=m_state.get("profile");auto newProfile=next.get("profile");
 if(oldProfile&&newProfile){auto oldLevel=jsonutil::integerOr(oldProfile.unwrap(),"level",1),newLevel=jsonutil::integerOr(newProfile.unwrap(),"level",1);
  if(newLevel>oldLevel)Notification::create(fmt::format("LEVEL UP! Nivel {} | Premio disponivel em Levels",newLevel),NotificationIcon::Success,6.f)->show();
  auto gained=jsonutil::integerOr(newProfile.unwrap(),"lifetime_xp")-jsonutil::integerOr(oldProfile.unwrap(),"lifetime_xp");
  auto oldQuest=m_state.get("quest");auto newQuest=next.get("quest");
  if(gained>0&&oldQuest&&newQuest&&jsonutil::stringOr(oldQuest.unwrap(),"run_id")!=jsonutil::stringOr(newQuest.unwrap(),"run_id")){
   auto name=jsonutil::stringOr(oldQuest.unwrap(),"title","Quest");
   Notification::create(fmt::format("Missao concluida! {} | +{} XP +{} Tokens",name.substr(0,48),gained,jsonutil::integerOr(oldQuest.unwrap(),"tokens")),NotificationIcon::Success,5.f)->show();
  }
 }
 m_state=next;Mod::get()->setSavedValue("geobr-v2-state",m_state);persistPassState();
}
void GeoPassService::refresh(){
 if(m_refreshing)return;m_refreshing=true;
 m_refresh.rpc("geobr_state_v2",matjson::Value::object(),[this](auto r){m_refreshing=false;if(!r.success)return;ingest(r.value);
  auto name=PlayerService::current().username;
  auto gm=GameManager::sharedState();auto c1=gm->colorForIdx(gm->getPlayerColor()),c2=gm->colorForIdx(gm->getPlayerColor2());
  auto packed=[](ccColor3B c){return (int(c.r)<<16)|(int(c.g)<<8)|int(c.b);};
  auto profile=matjson::makeObject({{"p_name",name.substr(0,40)},{"p_icon",std::clamp(gm->getPlayerFrame(),1,std::max(1,gm->countForType(IconType::Cube)))},{"p_color1",packed(c1)},{"p_color2",packed(c2)}});
  std::string slug;if(auto active=more_icons::activeIcon(IconType::Cube))slug=active->getName();profile["p_icon_slug"]=slug;
  // RC9: the whole icon set, so other players see it exactly as configured.
  profile["p_glow"]=gm->getPlayerGlow();
  profile["p_glow_color"]=packed(gm->colorForIdx(gm->getPlayerGlowColor()));
  auto icon=[&](IconType type,int id){return std::clamp(id,1,std::max(1,gm->countForType(type)));};
  profile["p_icons"]=matjson::makeObject({{"ship",icon(IconType::Ship,gm->getPlayerShip())},{"ball",icon(IconType::Ball,gm->getPlayerBall())},
   {"ufo",icon(IconType::Ufo,gm->getPlayerBird())},{"wave",icon(IconType::Wave,gm->getPlayerDart())},{"robot",icon(IconType::Robot,gm->getPlayerRobot())},
   {"spider",icon(IconType::Spider,gm->getPlayerSpider())},{"swing",icon(IconType::Swing,gm->getPlayerSwing())},{"jetpack",icon(IconType::Jetpack,gm->getPlayerJetpack())}});
  auto fingerprint=profile.dump();
  if(!name.empty()&&fingerprint!=m_sentName){m_sentName=fingerprint;m_name.rpc("geobr_profile_sync_v3",profile,[this](auto r){if(r.success)ingest(r.value);else m_sentName.clear();});}

 });
}
bool GeoPassService::matchesQuest(matjson::Value const& event)const{
 auto value=m_state.get("quest");if(!value||!value.unwrap().isObject())return false;auto q=value.unwrap();
 if(jsonutil::stringOr(event,"run")!=jsonutil::stringOr(q,"run_id"))return false;
 auto kind=jsonutil::stringOr(q,"kind"),eventKind=jsonutil::stringOr(event,"kind");
 if(kind!="reach"&&kind!=eventKind)return false;
 for(auto key:{"scope","difficulty","mode","platformer"}){auto wanted=jsonutil::stringOr(q,key);if(wanted!="any"&&wanted!=jsonutil::stringOr(event,key))return false;}
 auto pct=jsonutil::integerOr(event,"percent");return pct>=jsonutil::integerOr(q,"min_percent")&&pct<=jsonutil::integerOr(q,"max_percent",100);
}
bool GeoPassService::syncingQuest()const{for(auto const& e:m_queue)if(matchesQuest(e))return true;return false;}
std::string GeoPassService::questStatus()const{if(!syncingQuest())return "Progresso confirmado pelo servidor";if(m_retries>0)return "Sem conexao: progresso salvo para reenviar";return m_sending?"Enviando progresso da missao...":"Aguardando sincronizacao...";}
void GeoPassService::drain(float){sendNext();}
void GeoPassService::activity(matjson::Value event){
 if(!event.isObject()||!event.contains("id")||m_queue.size()>=128)return;
 bool quest=matchesQuest(event);bool drop=jsonutil::stringOr(event,"kind")=="complete"&&jsonutil::stringOr(event,"mode")=="normal";
 if(!quest&&!drop)return; // Avoid sending every unrelated death ahead of a completed mission.
 if(quest)m_queue.insert(m_queue.begin()+(m_sending?1:0),event);else m_queue.push_back(event);Mod::get()->setSavedValue("activity-pending-v2",matjson::Value(m_queue));persistPassState();
 if(quest&&m_pendingNoticeRun!=currentRun()){m_pendingNoticeRun=currentRun();Notification::create("Verificando progresso da missao...",NotificationIcon::Info,1.5f)->show();}
 sendNext();
}
void GeoPassService::sendNext(){
 if(m_sending||m_queue.empty()||std::chrono::steady_clock::now()<m_retryAt)return;
 // Old RC3 queues can contain many irrelevant deaths. Discard those locally;
 // completion events remain eligible for their independently deduplicated drop.
 while(!m_queue.empty()&&!matchesQuest(m_queue.front())&&jsonutil::stringOr(m_queue.front(),"kind")!="complete")m_queue.erase(m_queue.begin());
 if(m_queue.empty()){Mod::get()->setSavedValue("activity-pending-v2",matjson::Value(m_queue));persistPassState();return;}
 m_sending=true;auto event=m_queue.front();
 m_complete.rpc("geobr_activity_v4",matjson::makeObject({{"p_event",event}}),[this](auto r){
  m_sending=false;bool rejected=r.httpStatus>=400&&r.httpStatus<500&&r.httpStatus!=401&&r.httpStatus!=429;
  if(!r.success&&!rejected){m_retryAt=std::chrono::steady_clock::now()+std::chrono::seconds(std::min(10,1<<std::min(++m_retries,4)));return;}
  m_retries=0;m_retryAt={};
  if(r.success){auto state=r.value.get("state");if(state)ingest(state.unwrap());auto drop=r.value.get("drop");if(drop&&drop.unwrap().isObject())Notification::create("Voce ganhou uma caixa! Abra em GeoPass > Caixas",NotificationIcon::Success,5.f)->show();}
  if(!m_queue.empty())m_queue.erase(m_queue.begin());Mod::get()->setSavedValue("activity-pending-v2",matjson::Value(m_queue));persistPassState();
  // Next event is dispatched by the 1-second drain, outside this request callback.
 });
}
namespace {
// A notice from the staff: title, picture (if any) and text, on top of any menu.
class NoticePopup final:public geode::Popup{
public:
    static NoticePopup* create(std::string const& title,std::string const& text,std::string const& image){auto p=new NoticePopup;if(p->init(title,text,image)){p->autorelease();return p;}delete p;return nullptr;}
private:
    bool init(std::string const& title,std::string const& text,std::string const& image){
        bool pic=!image.empty();
        if(!Popup::init(380.f,pic?270.f:170.f))return false;
        setTitle(title.empty()?"Aviso":title);
        auto size=m_mainLayer->getContentSize();
        if(pic){auto img=LazySprite::create({340.f,140.f},true);img->setAutoResize(true);img->setPosition({size.width/2,size.height-110.f});
            img->loadFromUrl(GeoBRConfig::supabaseUrl()+"/storage/v1/object/public/geobr-news-images/"+image);m_mainLayer->addChild(img);}
        auto body=SimpleTextArea::create(text,"chatFont.fnt",.6f,340.f);body->setAlignment(kCCTextAlignmentCenter);body->setWrappingMode(WrappingMode::WORD_WRAP);body->setMaxLines(4);
        body->setPosition({size.width/2,pic?44.f:size.height/2-10.f});m_mainLayer->addChild(body);
        return true;
    }
};
}
// Staff notices and news reach everyone with the mod open (global): a notice with
// a picture opens as a window, the rest as a toast. A new device does not replay
// the old ones: it starts from the newest.
void GeoPassService::notices(){
    if(m_notices.isPending())return;
    auto time=std::time(nullptr);char stamp[32];std::strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%SZ",std::gmtime(&time));
    auto cursor=Mod::get()->getSavedValue<int64_t>("announcement-cursor",0);
    std::vector<std::pair<std::string,std::string>> q{{"published","eq.true"},{"expires_at",std::string("gt.")+stamp}};
    if(cursor>0){q.push_back({"id","gt."+std::to_string(cursor)});q.push_back({"order","id.asc"});q.push_back({"limit","3"});}
    else{q.push_back({"order","id.desc"});q.push_back({"limit","1"});}
    m_notices.spawn("GeoBR - avisos",ApiClient::getSupabase(GeoBRConfig::supabaseTableUrl("geobr_announcements"),q),[cursor](auto response){
        if(PlayLayer::get())return;
        auto result=CloudService::decode(response);
        if(!result.success||!result.value.isArray())return;
        auto latest=std::max<int64_t>(cursor,1);
        for(auto const& item:result.value.asArray().unwrap()){
            auto image=jsonutil::stringOr(item,"image_path");
            if(!image.empty()){if(auto p=NoticePopup::create(jsonutil::stringOr(item,"title"),jsonutil::stringOr(item,"message"),image))p->show();}
            else Notification::create(jsonutil::stringOr(item,"title")+": "+jsonutil::stringOr(item,"message"),NotificationIcon::Info,8.f)->show();
            latest=std::max(latest,jsonutil::integerOr(item,"id"));
        }
        if(latest!=cursor){Mod::get()->setSavedValue("announcement-cursor",latest);persistPassState();}
    });
    // News: a global toast when a new one is published (first run: start from the newest, silently).
    auto news=Mod::get()->getSavedValue<int64_t>("news-cursor",0);
    m_newsCheck.call("GET",news>0?fmt::format("/rest/v1/geobr_news?select=id,title&published=eq.true&id=gt.{}&order=id.asc&limit=3",news)
                                 :std::string("/rest/v1/geobr_news?select=id&published=eq.true&order=id.desc&limit=1"),{},[news](auto r){
        if(!r.success||!r.value.isArray())return;
        auto latest=std::max<int64_t>(news,1);
        for(auto const& n:r.value.asArray().unwrap()){
            if(news>0&&!PlayLayer::get())Notification::create("Nova noticia: "+jsonutil::stringOr(n,"title"),NotificationIcon::Info,6.f)->show();
            latest=std::max(latest,jsonutil::integerOr(n,"id"));
        }
        if(latest!=news)Mod::get()->setSavedValue("news-cursor",latest);
    });
}
std::string GeoPassService::rewardLabel(matjson::Value const& r){
    auto kind=jsonutil::stringOr(r,"kind");auto amount=jsonutil::integerOr(r,"amount");
    if(kind=="crate"){const char* names[]={"Comum","Incomum","Rara","Epica","Lendaria","Mitica"};return std::string("Caixa ")+names[std::clamp<int>(jsonutil::integerOr(r,"crate_rarity",1),1,6)-1];}
    if(kind=="theme"||kind=="title")return jsonutil::stringOr(r,"name",jsonutil::stringOr(r,"item_id"));
    if(kind=="arrow"){auto skin=arrow::skinFromItem(jsonutil::stringOr(r,"item_id"));return skin?fmt::format("Arrow {}",arrow::skinInfo(skin).name):std::string("Arrow");}
    if(kind=="quest_skip")return fmt::format("{} Skips",amount);
    if(kind=="skin")return fmt::format("Skin {} #{}",jsonutil::stringOr(r,"icon_type"),jsonutil::integerOr(r,"icon_id"));
    return fmt::format("{} {}",amount,kind=="tokens"?"Tokens":kind=="orbs"?"Orbs":"Estrelas");
}
void GeoPassService::syncRewards(){if(m_syncing)return;m_syncing=true;rewardPage(0);}
void GeoPassService::rewardPage(int offset){
    m_rewards.call("GET",fmt::format("/rest/v1/geobr_reward_claims?select=id,reward&order=created_at.asc,id.asc&limit=100&offset={}",offset),{},[this,offset](auto result){
        if(!result.success||!result.value.isArray()){m_syncing=false;return;}
        auto rows=result.value.asArray().unwrap();auto gm=GameManager::sharedState();auto stats=GameStatsManager::sharedState();bool changed=false;
        for(auto const& row:rows){
            auto id=jsonutil::stringOr(row,"id");auto object=row.get("reward");if(id.empty()||!object)continue;
            auto const& reward=object.unwrap();auto kind=jsonutil::stringOr(reward,"kind");
            auto receipt="geobr-reward-"+id;
            if(kind=="tokens"||gm->getGameVariable(receipt.c_str()))continue;
            int amount=static_cast<int>(jsonutil::integerOr(reward,"amount"));if(amount<1||amount>100000)continue;
            if(kind=="orbs"){stats->incrementStat("14",amount);stats->incrementStat("22",amount);}
            else if(kind=="stars")stats->incrementStat("6",amount);
            else if(kind=="skin"){
                auto typeName=jsonutil::stringOr(reward,"icon_type");IconType type=IconType::Cube;
                if(typeName=="ship")type=IconType::Ship;else if(typeName=="ball")type=IconType::Ball;else if(typeName=="ufo")type=IconType::Ufo;
                else if(typeName=="wave")type=IconType::Wave;else if(typeName=="robot")type=IconType::Robot;else if(typeName=="spider")type=IconType::Spider;
                else if(typeName=="swing")type=IconType::Swing;else if(typeName=="jetpack")type=IconType::Jetpack;
                int icon=static_cast<int>(jsonutil::integerOr(reward,"icon_id"));
                if(icon<1||icon>gm->countForType(type))continue;
                gm->unlockIcon(icon,type);
            }else continue;
            gm->setGameVariable(receipt.c_str(),true);changed=true;
        }
        // Reward and receipt are committed together to GD's own save, so restarting does not repeat grants.
        if(changed){gm->save();Notification::create("Recompensas do GeoPass recebidas!",NotificationIcon::Success)->show();}
        if(rows.size()==100)rewardPage(offset+100);else m_syncing=false;
    });
}
}
