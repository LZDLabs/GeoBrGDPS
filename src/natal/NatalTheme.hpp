#pragma once
// Christmas look of the GDPS menus (only while the Natal event is open for the
// player and the "Decoracao de Natal" setting is on; the editor and gameplay
// are never touched, and on the official servers these hooks are off).
// Everything is plain sprites/actions with the mod's own art: snow in three
// depth layers, twinkling sparkles, a candy-cane frame, lights, snow on the
// buttons and on the ground, a Santa hat on the title, red/green backgrounds.
#include <Geode/Geode.hpp>

namespace geobr::natal::theme {

bool on();

inline constexpr cocos2d::ccColor3B Red{168, 24, 40};
inline constexpr cocos2d::ccColor3B Green{22, 104, 56};

/** Falling snow (3 layers) + sparkles on a line (y < 0: none). */
void snow(cocos2d::CCNode* layer, float sparkleLine = -1.f);
/** String of lights along the top edge. */
void lights(cocos2d::CCNode* layer);
/** Candy-cane stripes on the four edges; corners = presents, gingerbread, snowman, candy cane, holly. */
void frame(cocos2d::CCNode* layer, bool corners);
/** Snow on top of a button: round buttons get the round cap, others the flat one. */
void cap(cocos2d::CCNode* button);
/** Flat snow over the middle part of a node's top (the top arm of the menu crosses). */
void capFlat(cocos2d::CCNode* node, float widthRatio);
/** Sets the colour of a menu's background sprite (GD's tinted gradient). */
void recolorBackground(cocos2d::CCNode* layer, cocos2d::ccColor3B color);
/** A full red gradient behind a page of the mod (z 0, add it before the content). */
cocos2d::CCNode* gradient(cocos2d::CCSize size);
/** Holly on the top-right corner of a card. */
void holly(cocos2d::CCNode* card);

/** Art used to show a theme/frame in lists (theme background or the frame's main piece). */
cocos2d::CCNode* cosmeticIcon(std::string const& id, float size);
/** Draws a Christmas profile frame on a panel (antlers on top, wreath, lights, candy cane, snow); empty id removes it. */
void profileFrame(cocos2d::CCNode* panel, std::string const& id);

/** Main menu: everything above + snow bank on the ground and the hat on the title. */
void mainMenu(cocos2d::CCLayer* menu);

} // namespace geobr::natal::theme
