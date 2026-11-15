#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <string>

// GeoBR design system (RC6): Geometry Dash look with nearly square frames,
// a thin white border, a black contour and an offset drop shadow. Text uses
// bigFont (Pusab) for titles/buttons and chatFont for body copy.
namespace geobr::ui {

namespace palette {
inline constexpr cocos2d::ccColor3B Green {64, 186, 64};
inline constexpr cocos2d::ccColor3B Blue {40, 128, 220};
inline constexpr cocos2d::ccColor3B Cyan {0, 172, 214};
inline constexpr cocos2d::ccColor3B Slate {58, 70, 118};
inline constexpr cocos2d::ccColor3B Red {206, 58, 58};
inline constexpr cocos2d::ccColor3B Gold {222, 160, 30};
inline constexpr cocos2d::ccColor3B Night {24, 28, 54};
inline constexpr cocos2d::ccColor3B TextGold {255, 226, 120};
inline constexpr cocos2d::ccColor3B TextSoft {190, 214, 240};
} // namespace palette

// Resizable GD-style frame. setColor/setOpacity tint only the fill.
class Frame final : public cocos2d::CCNodeRGBA {
public:
    static Frame* create(cocos2d::CCSize size, cocos2d::ccColor3B fill, GLubyte opacity = 255, bool shadow = true);
    void setContentSize(cocos2d::CCSize const& size) override;
    void setColor(cocos2d::ccColor3B const& color) override;
    void setOpacity(GLubyte opacity) override;
    void updateDisplayedOpacity(GLubyte parentOpacity) override;
    void setBorderColor(cocos2d::ccColor3B color);

private:
    bool init(cocos2d::CCSize size, cocos2d::ccColor3B fill, GLubyte opacity, bool shadow);
    void redraw();
    cocos2d::CCDrawNode* m_draw = nullptr;
    cocos2d::ccColor3B m_border {255, 255, 255};
    bool m_shadow = true;
};

// Menu item that dims itself when disabled (GD's own item does not).
class Button final : public CCMenuItemSpriteExtra {
public:
    static Button* create(cocos2d::CCNode* sprite, cocos2d::CCObject* target, cocos2d::SEL_MenuHandler callback);
    void setEnabled(bool enabled) override;
};

cocos2d::CCLabelBMFont* makeLabel(std::string const&, char const*, float, cocos2d::ccColor3B = {255, 255, 255});
Frame* makePanel(cocos2d::CCSize, cocos2d::ccColor3B = {20, 45, 84}, GLubyte = 210);
// `bg` keeps the RC5 call sites: GJ_button_01 green, 02/03 blue/cyan,
// 04 slate (secondary), 05 gold, 06 red (danger).
CCMenuItemSpriteExtra* makeTextButton(std::string const&, int, char const*, cocos2d::CCObject*, cocos2d::SEL_MenuHandler, float = .45f, float = 28.f);
cocos2d::ccColor3B buttonColor(char const* bg);
void clearScroll(geode::ScrollLayer*);
std::string compactNumber(std::int64_t);
// Safe substring: never throws when `pos` is beyond the end.
std::string slice(std::string const& value, std::size_t pos, std::size_t count = std::string::npos);

} // namespace geobr::ui
