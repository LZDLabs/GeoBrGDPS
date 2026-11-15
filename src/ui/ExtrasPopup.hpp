#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
namespace geobr {class ExtrasPopup final:public geode::Popup {CloudRequest m_request;cocos2d::CCNode* m_view=nullptr;cocos2d::CCLabelBMFont* m_status=nullptr;matjson::Value m_rows;int m_tab=0;bool init()override;void render();void onTab(cocos2d::CCObject*);void onDensity(cocos2d::CCObject*);void onHistory(cocos2d::CCObject*);void onCollection(cocos2d::CCObject*);public:static ExtrasPopup* create();};}
