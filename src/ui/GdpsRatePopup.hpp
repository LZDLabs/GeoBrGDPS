#pragma once
// RobTop-style "Rate Stars" for GDPS staff: stars 1..10, the difficulty face
// on the right cycles Rated > Featured > Epic > Legendary > Mythic when tapped,
// Demon difficulty when 10 stars, coins check. Used by the level's Mod button
// and by the admin panel; the server checks the permissions again.
#include "../gdps/GdpsService.hpp"

#include <Geode/binding/GJDifficultySprite.hpp>
#include <Geode/ui/Popup.hpp>

#include <functional>
#include <string>

namespace geobr {

class GdpsRatePopup final : public geode::Popup {
public:
    static GdpsRatePopup* create(int levelID, std::string name, int stars = 0, int tier = 0, std::function<void()> done = {});

private:
    int m_level = 0, m_stars = 0, m_tier = 0, m_demon = 3;
    bool m_coins = false, m_busy = false;
    std::string m_name;
    std::function<void()> m_done;
    cocos2d::CCMenu* m_menu = nullptr;
    GJDifficultySprite* m_face = nullptr;
    cocos2d::CCLabelBMFont* m_tierLabel = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    gdps::Request m_action;

    bool init(int levelID, std::string name, int stars, int tier, std::function<void()> done);
    void render();
    void onStar(cocos2d::CCObject*);
    void onFace(cocos2d::CCObject*);
    void onDemon(cocos2d::CCObject*);
    void onCoins(cocos2d::CCObject*);
    void onSubmit(cocos2d::CCObject*);
    void onCancel(cocos2d::CCObject*);
};

} // namespace geobr
