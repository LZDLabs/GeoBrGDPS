#pragma once
#include "../models/NewsItem.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
namespace geobr {class NewsTab final:public cocos2d::CCLayer{public:static NewsTab*create(cocos2d::CCSize);private:geode::ScrollLayer*m_list=nullptr;cocos2d::CCNode*m_state=nullptr;geode::async::TaskHolder<geode::utils::web::WebResponse>m_request;bool init(cocos2d::CCSize);void onRefresh(cocos2d::CCObject*);void refresh();void poll(float);void showState(std::string const&,bool,bool=false);void render(std::vector<NewsItem>const&);cocos2d::CCNode*card(NewsItem const&)const;};}
