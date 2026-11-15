#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
namespace geobr {
class AdminSafetyPopup final:public geode::Popup {
 CloudRequest m_request;geode::ScrollLayer* m_list=nullptr;cocos2d::CCLabelBMFont* m_status=nullptr;matjson::Value m_rows;int m_tab=0;bool m_busy=false;
 bool init()override;void load();void render();void onTab(cocos2d::CCObject*);void onRow(cocos2d::CCObject*);void onNew(cocos2d::CCObject*);void onRefresh(cocos2d::CCObject*);
public:static AdminSafetyPopup* create();
};}
