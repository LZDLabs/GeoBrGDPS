#pragma once
#include <Geode/Geode.hpp>
#include <matjson.hpp>
class SimplePlayer;
namespace geobr::visual {
struct Palette {cocos2d::ccColor3B top,bottom,accent;};
void fitPopup(cocos2d::CCNode*,cocos2d::CCSize);
// RC6 popup chrome: replaces the rounded GD square with a GeoBR frame.
void stylePopup(cocos2d::CCNode* mainLayer,cocos2d::CCNode* nativeBackground,cocos2d::CCSize size,cocos2d::ccColor3B color);
bool hasThemeArt(std::string const& style);
Palette palette(std::string const& style);
cocos2d::CCNode* theme(cocos2d::CCSize,std::string const& style);
cocos2d::CCNode* bar(cocos2d::CCSize,float ratio,cocos2d::ccColor3B color={78,240,208});
cocos2d::CCNode* tile(char const* texture,std::string const& label,float size);
cocos2d::CCNode* rewardIcon(matjson::Value const&,float size);
cocos2d::CCNode* avatar(matjson::Value const&,float size);
// RC9: full player icons (glow + every gamemode) synced by profile_sync_v3.
void applyGlow(SimplePlayer* player,matjson::Value const& profile);
cocos2d::CCNode* playerIcon(matjson::Value const& profile,IconType type,float size);
std::string uuid();
}
