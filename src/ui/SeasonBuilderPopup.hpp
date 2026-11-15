#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>
namespace geobr {
class SeasonBuilderPopup final:public geode::Popup {
 CloudRequest m_request;geode::TextInput* m_name=nullptr;geode::TextInput* m_numbers[4]{};
 geode::ScrollLayer* m_preview=nullptr;cocos2d::CCLabelBMFont* m_status=nullptr;matjson::Value m_draft;bool m_busy=false;
 bool init()override;void onGenerate(CCObject*);void onPublish(CCObject*);void render();
public:static SeasonBuilderPopup* create();
};}
