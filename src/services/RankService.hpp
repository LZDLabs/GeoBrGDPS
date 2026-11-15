#pragma once
#include "ApiClient.hpp"
#include <Geode/Geode.hpp>
#include <map>
namespace geobr {
class RankService final:public cocos2d::CCNode {
 geode::async::TaskHolder<geode::utils::web::WebResponse> m_task;std::map<int,int> m_ranks;
 
public:static RankService* get();int position(int id)const;void start();void refresh(float=0);
};
}
