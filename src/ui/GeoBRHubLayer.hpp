#pragma once
#include <Geode/ui/Popup.hpp>
#include <array>
namespace geobr {
class GeoBRHubLayer final:public geode::Popup {
 std::array<cocos2d::CCNode*,2> m_tabs{};std::array<CCMenuItemSpriteExtra*,2> m_buttons{};
 bool init()override;void selectTab(int);void onTab(cocos2d::CCObject*);void onShortcut(cocos2d::CCObject*);
 public:static GeoBRHubLayer* create();
};
}
