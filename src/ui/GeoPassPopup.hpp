#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
namespace geobr {
class GeoPassPopup final:public geode::Popup {
 CloudRequest m_request;cocos2d::CCNode* m_view=nullptr;cocos2d::CCLabelBMFont* m_status=nullptr;
 matjson::Value m_state;bool m_busy=false,m_positioned=false;int m_page=0;int64_t m_revision=-1;
 bool init()override;void poll(float);void render();void refresh();void finish(ApiJsonResult);
 void onProgress(cocos2d::CCObject*);void onClaim(cocos2d::CCObject*);void onPage(cocos2d::CCObject*);void onQuest(cocos2d::CCObject*);
public:static GeoPassPopup* create();
};}
