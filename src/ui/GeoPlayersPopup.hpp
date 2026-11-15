#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
namespace geobr {
class GeoPlayersPopup final:public geode::Popup {
 CloudRequest m_request;
 geode::TextInput* m_input=nullptr;
 cocos2d::CCNode* m_view=nullptr;
 cocos2d::CCLabelBMFont* m_status=nullptr;
 matjson::Value m_data=matjson::Value::object();
 int m_page=0;bool m_busy=false;
 bool init();void fetch();void render();
 void onSearch(cocos2d::CCObject*);void onPage(cocos2d::CCObject*);void onProfile(cocos2d::CCObject*);void onGroups(cocos2d::CCObject*);
public:static GeoPlayersPopup* create();
};}
