#pragma once

#include "ArrowSkins.hpp"
#include "../services/CloudService.hpp"

#include <Geode/ui/Popup.hpp>

#include <array>
#include <string>

namespace geobr::arrow {

// Arrow showcase: animated 45° demo, the four skins and the Col 1 / Col 2 /
// Glow channels. Editable for the local player, read-only for others.
class ArrowPopup final : public geode::Popup {
public:
    static ArrowPopup* createLocal();
    static ArrowPopup* createRemote(std::string const& name, Appearance const& appearance);

private:
    Appearance m_look;
    bool m_editable = false;
    std::string m_owner;
    cocos2d::CCNode* m_cards = nullptr;
    cocos2d::CCNode* m_demoTrack = nullptr;
    cocos2d::CCNode* m_demoPortal = nullptr;
    ArrowIcon* m_demoIcon = nullptr;
    cocos2d::CCMotionStreak* m_demoTrail = nullptr;
    cocos2d::CCLabelBMFont* m_glowLabel = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    float m_demoY = 0.f;
    int m_demoDirection = 1;

    bool init(std::string owner, Appearance appearance, bool editable);
    void buildDemo();
    void buildCards();
    void buildChannels();
    void tickDemo(float dt);
    void onDemoTap(cocos2d::CCObject*);
    void onSkin(cocos2d::CCObject*);
    void onGlowMode(cocos2d::CCObject*);
};

} // namespace geobr::arrow
