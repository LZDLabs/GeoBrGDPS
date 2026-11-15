#include "RankService.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../models/JsonModelUtils.hpp"
using namespace geode::prelude;
namespace geobr {
RankService* RankService::get(){static auto p=[](){auto s=new RankService;s->init();return s;}();return p;}
int RankService::position(int id)const{auto it=m_ranks.find(id);return it==m_ranks.end()?0:it->second;}
void RankService::start(){auto saved=Mod::get()->getSavedValue<matjson::Value>("rank-cache",matjson::Value::array());if(saved.isArray())for(auto const& row:saved.asArray().unwrap())m_ranks[jsonutil::integerOr(row,"level_id")]=jsonutil::integerOr(row,"position");CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(RankService::refresh),this,60.f,false);refresh(0);}
void RankService::refresh(float){if(m_task.isPending())return;m_task.spawn("GeoBR - ranking",ApiClient::getSupabase(GeoBRConfig::supabaseTableUrl("geobr_level_ranks"),{{"select","level_id,position"},{"published","eq.true"},{"order","position.asc"}}),[this](auto response){auto r=ApiClient::decodeJson(response);if(!r.success||!r.value.isArray())return;m_ranks.clear();for(auto const& row:r.value.asArray().unwrap())m_ranks[jsonutil::integerOr(row,"level_id")]=jsonutil::integerOr(row,"position");Mod::get()->setSavedValue("rank-cache",r.value);});}
}
