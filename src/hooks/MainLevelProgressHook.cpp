#include "../services/MainLevelProgress.hpp"
#include "../services/GeoPassService.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../ui/GeoBRVisuals.hpp"
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
using namespace geode::prelude;
class $modify(GeoBRMainLevelGameManager,GameManager){
 void reportPercentageForLevel(int id,int pct,bool practice){GameManager::reportPercentageForLevel(id,pct,practice);geobr::MainLevelProgress::report(id,pct,practice);}
};
class $modify(GeoBRPassPlayLayer,PlayLayer){
 struct Fields{std::string attempt,run;bool deathSent=false,completeSent=false,reachSent=false;float elapsed=0;};
 void beginQuestAttempt(){m_fields->attempt=geobr::visual::uuid();m_fields->run=geobr::GeoPassService::get()->currentRun();m_fields->elapsed=0;m_fields->deathSent=false;m_fields->completeSent=false;m_fields->reachSent=false;}
 bool init(GJGameLevel* level,bool replay,bool empty){if(!PlayLayer::init(level,replay,empty))return false;beginQuestAttempt();return true;}
 void resetLevel(){PlayLayer::resetLevel();beginQuestAttempt();}
 matjson::Value questEvent(std::string const& kind,int pct){
  if(!m_level||m_isTestMode)return {};int id=m_level->m_levelID.value();if(id<=0)return {};std::string scope;
  if(geobr::MainLevelProgress::isGeoBRLocalID(id)){scope="main";id-=geobr::MainLevelProgress::LocalIDBase;}
  else if(m_level->m_levelType==GJLevelType::Main)scope="official";
  else if(m_level->m_levelType==GJLevelType::Saved||m_level->m_levelType==GJLevelType::SearchResult)scope="online";else return {};
  std::string difficulty="unrated";
  if(m_level->m_autoLevel)difficulty="auto";else if(m_level->m_demon.value())difficulty="demon";
  else{int d=static_cast<int>(m_level->m_difficulty);if(scope=="online"&&m_level->m_ratings>0)d=m_level->m_ratingsSum/m_level->m_ratings;
   if(d>=1&&d<=5){const char* names[]={"easy","normal","hard","harder","insane"};difficulty=names[d-1];}else if(d>=6&&d<=10)difficulty="demon";}
  return matjson::makeObject({{"id",geobr::visual::uuid()},{"attempt",m_fields->attempt},{"run",geobr::GeoPassService::get()->currentRun()},{"elapsed",m_fields->elapsed},{"kind",kind},{"scope",scope},{"level",scope+":"+std::to_string(id)},{"difficulty",difficulty},{"mode",m_isPracticeMode?"practice":"normal"},{"platformer",m_level->isPlatformer()?"platformer":"classic"},{"percent",std::clamp(pct,0,100)}});
 }
 void destroyPlayer(PlayerObject* player,GameObject* object){
  bool record=player&&(player==m_player1||player==m_player2)&&!player->m_isDead&&!m_fields->deathSent;auto event=record?questEvent("death",getCurrentPercentInt()):matjson::Value();
  PlayLayer::destroyPlayer(player,object);if(record&&player->m_isDead){m_fields->deathSent=true;geobr::GeoPassService::get()->activity(event);}
 }
 void levelComplete(){auto event=m_fields->completeSent?matjson::Value():questEvent("complete",100);m_fields->completeSent=true;PlayLayer::levelComplete();geobr::GeoPassService::get()->activity(event);}
 void postUpdate(float dt){
  PlayLayer::postUpdate(dt);if(!m_isPaused)m_fields->elapsed+=std::clamp(dt,0.f,.25f);if(m_fields->run!=geobr::GeoPassService::get()->currentRun()){m_fields->run=geobr::GeoPassService::get()->currentRun();m_fields->reachSent=false;}if(m_fields->reachSent||!m_player1||m_player1->m_isDead||m_fields->run.empty()||m_isTestMode)return;
  auto q=geobr::GeoPassService::get()->state().get("quest");if(!q||geobr::jsonutil::stringOr(q.unwrap(),"kind")!="reach"||geobr::jsonutil::stringOr(q.unwrap(),"run_id")!=m_fields->run)return;
  int pct=getCurrentPercentInt();if(pct>=geobr::jsonutil::integerOr(q.unwrap(),"min_percent")){m_fields->reachSent=true;geobr::GeoPassService::get()->activity(questEvent("reach",pct));}
 }
};
