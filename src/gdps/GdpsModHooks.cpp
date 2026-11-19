// GDPS buttons added to GD screens:
// * main menu: the server button (Oficial / GDPS), only with geobr.gdps installed;
// * level page and profile: a "Mod" button, only on the GDPS and only for staff.
// They are appended to GD's own button menus through the node-ids layouts (so
// other mods' buttons are arranged together, not overlapped) or, when a menu
// has no layout, placed in a separate geobr-* menu. Native nodes are untouched.
#include "../services/PresenceService.hpp"
#include "GdpsService.hpp"
#include "../services/UpdateService.hpp"
#include "../ui/GdpsPopups.hpp"
#include "../ui/UIHelpers.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/ProfilePage.hpp>

using namespace geode::prelude;
using namespace geobr;

namespace {

CCMenuItemSpriteExtra* smallButton(char const* text, char const* bg, CCObject* target, SEL_MenuHandler handler, char const* id) {
    auto sprite = ButtonSprite::create(text, 50, true, "bigFont.fnt", bg, 26, .5f);
    sprite->setScale(.7f);
    auto button = CCMenuItemSpriteExtra::create(sprite, target, handler);
    button->setID(id);
    return button;
}

/** Appends to the first existing menu that has a layout; otherwise a separate menu at `fallback`. */
void place(CCNode* root, std::initializer_list<char const*> menus, CCMenuItemSpriteExtra* button, CCNode* fallbackParent, CCPoint fallback, char const* fallbackID) {
    for (auto id : menus) {
        if (auto menu = typeinfo_cast<CCMenu*>(root->getChildByIDRecursive(id)); menu && menu->getLayout()) {
            menu->addChild(button);
            menu->updateLayout();
            return;
        }
    }
    auto menu = CCMenu::create();
    menu->setID(fallbackID);
    menu->setPosition(fallback);
    menu->addChild(button);
    fallbackParent->addChild(menu, 50);
}

} // namespace

class $modify(GeoBRServerMenu, MenuLayer) {
    // Marked so main.cpp keeps it when GeoBR is off on the official server.
    static void onModify(auto& self) { (void)self.setHookPriority("MenuLayer::init", gdps::ServerMenuHookPriority); }

    bool init() {
        if (!MenuLayer::init()) return false;
        update::checkOnce();
        if (!gdps::coreInstalled()) return true;
        bool on = gdps::enabled();
        auto button = smallButton(on ? "GDPS" : "Oficial", on ? "GJ_button_01.png" : "GJ_button_04.png", this,
                                  menu_selector(GeoBRServerMenu::onGeoBRServer), "geobr-server-button");
        auto win = CCDirector::sharedDirector()->getWinSize();
        place(this, {"bottom-menu"}, button, this, {win.width / 2.f, 70.f}, "geobr-server-menu");
        if (gdps::loggedIn()) gdps::refreshMe();
        return true;
    }
    void onGeoBRServer(CCObject*) {
        if (auto p = ServerSwitchPopup::create()) p->show();
    }
};

class $modify(GeoBRGdpsLevelInfo, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        if (!gdps::loggedIn() || !level || level->m_levelID.value() <= 0) return true;
        auto add = [self = WeakRef<LevelInfoLayer>(this)] {
            auto layer = self.lock();
            if (!layer || !gdps::can("admin.panel") || layer->getChildByIDRecursive("geobr-gdps-level-mod")) return;
            auto button = smallButton("Mod", "GJ_button_06.png", layer.data(), menu_selector(GeoBRGdpsLevelInfo::onGeoBRMod), "geobr-gdps-level-mod");
            auto win = CCDirector::sharedDirector()->getWinSize();
            place(layer.data(), {"left-side-menu"}, button, layer.data(), {30.f, win.height / 2.f - 80.f}, "geobr-gdps-level-mod-menu");
        };
        if (gdps::me().get("permissions")) add();
        else gdps::refreshMe(add);
        return true;
    }
    void onGeoBRMod(CCObject*) {
        if (!m_level) return;
        if (auto p = LevelModPopup::create(m_level->m_levelID.value(), m_level->m_levelName)) p->show();
    }
};

class $modify(GeoBRGdpsProfile, ProfilePage) {
    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);
        presence::decorate(this, score);
        if (!gdps::loggedIn() || !score || score->m_accountID <= 0 || score->m_accountID == gdps::accountID()) return;
        if (!gdps::can("admin.panel") || m_mainLayer->getChildByIDRecursive("geobr-gdps-user-mod")) return;
        auto button = smallButton("Mod", "GJ_button_06.png", this, menu_selector(GeoBRGdpsProfile::onGeoBRMod), "geobr-gdps-user-mod");
        auto win = CCDirector::sharedDirector()->getWinSize();
        place(m_mainLayer, {"left-menu"}, button, m_mainLayer, {win.width / 2.f - 190.f, win.height / 2.f - 118.f}, "geobr-gdps-user-mod-menu");
    }
    void onGeoBRMod(CCObject*) {
        if (!m_score) return;
        if (auto p = UserModPopup::create(m_score->m_accountID, m_score->m_userName)) p->show();
    }
};
