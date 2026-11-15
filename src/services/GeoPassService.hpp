#pragma once
#include "CloudService.hpp"
#include <Geode/Geode.hpp>
namespace geobr {
class GeoPassService final:public cocos2d::CCNode {
 CloudRequest m_complete,m_drop,m_rewards,m_refresh,m_name,m_social;
 geode::async::TaskHolder<geode::utils::web::WebResponse> m_notices;
 std::vector<matjson::Value> m_queue;
 matjson::Value m_state=matjson::Value::object();
 bool m_started=false,m_sending=false,m_syncing=false,m_refreshing=false;std::string m_sentName;unsigned m_ticks=0;
 std::chrono::steady_clock::time_point m_retryAt{};int m_retries=0;std::string m_pendingNoticeRun;
 void drain(float);bool matchesQuest(matjson::Value const&)const;void tick(float);void sendNext();void notices();void rewardPage(int offset);
public:
 static GeoPassService* get();void start();void refresh();void ingest(matjson::Value const&);
 matjson::Value const& state()const{return m_state;}
 bool syncingQuest()const;std::string questStatus()const;
 std::string currentRun()const;void activity(matjson::Value);void syncRewards();
 static std::string rewardLabel(matjson::Value const&);
};}
