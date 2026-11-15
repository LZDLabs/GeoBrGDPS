#include "ArrowSkins.hpp"
#include "../services/GeoPassService.hpp"
#include "../ui/TierShopPopup.hpp"
#include "../ui/UIHelpers.hpp"

#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/binding/ListButtonBar.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/modify/GJGarageLayer.hpp>

#include <algorithm>
#include <array>

using namespace geode::prelude;
using namespace geobr::arrow;

namespace {

constexpr char TabID[] = "geobr-arrow-garage-tab";
constexpr int Columns = 8;
constexpr float Cell = 34.f;

bool insideListBar(CCNode* node, CCNode* root) {
    for (auto n = node ? node->getParent() : nullptr; n && n != root; n = n->getParent()) {
        if (typeinfo_cast<ListButtonBar*>(n)) return true;
    }
    return false;
}

template <class F>
void walk(CCNode* node, F&& visit) {
    if (!node) return;
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        if (!child) continue;
        if (!visit(child)) continue; // false: do not descend
        walk(child, visit);
    }
}

CCRect worldBox(CCNode* node) {
    auto box = node->boundingBox();
    auto parent = node->getParent();
    if (!parent) return box;
    auto a = parent->convertToWorldSpace(box.origin);
    auto b = parent->convertToWorldSpace({box.getMaxX(), box.getMaxY()});
    return CCRect(std::min(a.x, b.x), std::min(a.y, b.y), std::abs(b.x - a.x), std::abs(b.y - a.y));
}

CCRect unite(CCRect a, CCRect b) {
    if (a.size.width <= 0 && a.size.height <= 0) return b;
    float x0 = std::min(a.getMinX(), b.getMinX()), y0 = std::min(a.getMinY(), b.getMinY());
    float x1 = std::max(a.getMaxX(), b.getMaxX()), y1 = std::max(a.getMaxY(), b.getMaxY());
    return CCRect(x0, y0, x1 - x0, y1 - y0);
}

// Off-state colours, like GD's grey tab icons.
Appearance tabAppearance(bool selected) {
    auto look = ArrowSettings::localAppearance();
    if (!selected) {
        look.primary = {175, 175, 175};
        look.secondary = {255, 255, 255};
        look.glowEnabled = false;
    }
    return look;
}

} // namespace

// The Arrow lives in GD's icon kit like any other gamemode: one more tab in
// the gamemode row; selecting it swaps the icon grid for the Arrow skins.
class $modify(GeoBRArrowGarage, GJGarageLayer) {
    struct Fields {
        CCMenuItemSpriteExtra* tab = nullptr;
        ArrowIcon* tabIcon = nullptr;
        CCNode* page = nullptr;
        ArrowIcon* preview = nullptr;
        std::vector<std::pair<Ref<CCNode>, bool>> hidden;
        bool active = false;
    };

    bool init() {
        if (!GJGarageLayer::init()) return false;
        addArrowTab();
        return true;
    }

    void addArrowTab() {
        if (!m_tabButtons || m_tabButtons->count() == 0) return;
        // Place the tab right after the last native gamemode tab, using the
        // native spacing so it reads as part of the same row.
        CCNode* last = nullptr;
        CCNode* previous = nullptr;
        for (auto tab : CCArrayExt<CCNode*>(m_tabButtons)) {
            if (!tab) continue;
            previous = last;
            last = tab;
        }
        if (!last || !last->getParent()) return;
        float spacing = previous && previous->getParent() == last->getParent()
            ? std::abs(last->getPositionX() - previous->getPositionX()) : 30.f;
        spacing = std::clamp(spacing, 22.f, 44.f);
        auto size = last->getContentSize();

        auto holder = CCNode::create();
        holder->setContentSize(size);
        auto icon = ArrowIcon::create(tabAppearance(false));
        icon->setScale(std::min(size.width, size.height) / ArrowIcon::BaseSize * .78f);
        icon->setPosition(size / 2);
        holder->addChild(icon);
        auto tab = CCMenuItemSpriteExtra::create(holder, this, menu_selector(GeoBRArrowGarage::onArrowTab));
        tab->setID(TabID);
        tab->setPosition(last->getPosition() + CCPoint{spacing, 0.f});
        last->getParent()->addChild(tab);
        m_fields->tab = tab;
        m_fields->tabIcon = icon;
    }

    void onSelectTab(CCObject* sender) {
        if (m_fields->active) closeArrowPage();
        GJGarageLayer::onSelectTab(sender);
    }

    void onArrowTab(CCObject*) {
        if (m_fields->active) return;
        openArrowPage();
    }

    void openArrowPage() {
        auto& f = *m_fields.operator->();
        f.active = true;
        // Native tabs off, native grid/cursors/preview hidden (restored later).
        if (m_tabButtons) {
            for (auto tab : CCArrayExt<CCNode*>(m_tabButtons)) {
                if (auto toggler = typeinfo_cast<CCMenuItemToggler*>(tab)) toggler->toggle(false);
            }
        }
        f.hidden.clear();
        auto win = CCDirector::sharedDirector()->getWinSize();

        // RC7 hid m_iconSelection & co. and centred the grid on the bar's
        // bounding box. On the device the native grid stayed visible and the
        // Arrows ended up in the bottom-left corner: ListButtonBar sits at
        // (0,0) and lays its pages out internally, and the member offsets
        // cannot be verified for Android. The native pieces are now found by
        // walking the layer: icon bars by type, selection cursors and page
        // dots by sprite frame, and the grid area from the buttons on screen.
        std::vector<CCNode*> toHide;
        CCRect gridBox;
        CCNode* player = nullptr;
        walk(this, [&](CCNode* node) {
            if (node->getID() == "geobr-arrow-garage-page") return false;
            if (auto bar = typeinfo_cast<ListButtonBar*>(node)) {
                toHide.push_back(bar);
                walk(bar, [&](CCNode* item) {
                    if (typeinfo_cast<CCMenuItemSpriteExtra*>(item) && item->isVisible()) {
                        auto box = worldBox(item);
                        if (box.getMidX() > 0 && box.getMidX() < win.width && box.getMidY() > 0 && box.getMidY() < win.height) gridBox = unite(gridBox, box);
                        return false;
                    }
                    return true;
                });
                return false;
            }
            if (isSpriteFrameName(node, "GJ_select_001.png")) { toHide.push_back(node); return false; }
            if (typeinfo_cast<CCMenu*>(node) && (getChildBySpriteFrameName(node, "gj_navDotBtn_on_001.png") || getChildBySpriteFrameName(node, "gj_navDotBtn_off_001.png"))) {
                toHide.push_back(node);
                return false;
            }
            if (!player && typeinfo_cast<SimplePlayer*>(node) && !insideListBar(node, this)) { player = node; return false; }
            return true;
        });
        // Page arrows next to the grid (only when they are what the bindings say).
        for (CCNode* node : std::initializer_list<CCNode*>{m_leftArrow, m_rightArrow}) {
            if (node && typeinfo_cast<CCMenuItemSpriteExtra*>(node) && node->getParent()) toHide.push_back(node);
        }
        if (player) toHide.push_back(player);
        for (auto node : toHide) {
            if (std::ranges::any_of(f.hidden, [node](auto const& entry) { return entry.first.data() == node; })) continue;
            f.hidden.emplace_back(node, node->isVisible());
            node->setVisible(false);
        }
        if (f.tabIcon) f.tabIcon->setAppearance(tabAppearance(true));

        auto page = CCNode::create();
        page->setID("geobr-arrow-garage-page");
        addChild(page, 50);
        f.page = page;

        // Big preview where GD shows the selected icon.
        CCPoint previewPos {win.width / 2, win.height - 82.f};
        if (player && player->getParent()) previewPos = convertToNodeSpace(player->getParent()->convertToWorldSpace(player->getPosition()));
        auto preview = ArrowIcon::create(ArrowSettings::localAppearance());
        preview->setScale(1.6f);
        preview->setPosition(previewPos);
        preview->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCRotateTo::create(.8f, -45.f)),
            CCEaseSineInOut::create(CCRotateTo::create(.8f, 45.f)), nullptr
        )));
        page->addChild(preview, 2);
        f.preview = preview;

        // Grid where GD's icon list sits (fallback: GD's default spot).
        int rows = (static_cast<int>(Skins.size()) + Columns - 1) / Columns;
        CCSize gridSize {Columns * (Cell + 4.f) + 10.f, rows * (Cell + 6.f) + 30.f};
        CCPoint gridCenter {win.width / 2, win.height / 2 - 62.f};
        if (gridBox.size.width > 40 && gridBox.size.height > 20) {
            gridCenter = convertToNodeSpace({gridBox.getMidX(), gridBox.getMidY()});
            gridSize.width = std::max(gridSize.width, gridBox.size.width / getScale() + 16.f);
            gridSize.height = std::max(gridSize.height, gridBox.size.height / getScale() + 16.f);
        }
        gridSize.width = std::min(gridSize.width, win.width - 20.f);
        // Opaque GD-style panel over the native grid area, so nothing native
        // can show through even if GD rebuilds it.
        auto backing = geobr::ui::Frame::create(gridSize, {120, 64, 32}, 255, false);
        backing->setPosition(gridCenter);
        page->addChild(backing, -2);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        page->addChild(menu);
        for (size_t i = 0; i < Skins.size(); ++i) {
            int skin = Skins[i].id;
            bool owned = isOwned(skin);
            auto cell = CCNode::create();
            cell->setContentSize({Cell, Cell});
            auto look = ArrowSettings::localAppearance();
            look.skin = skin;
            if (!owned) {
                look.primary = {90, 90, 90};
                look.secondary = {150, 150, 150};
                look.glowEnabled = false;
            }
            auto icon = ArrowIcon::create(look);
            icon->setScale(.95f);
            icon->setPosition({Cell / 2, Cell / 2});
            cell->addChild(icon);
            if (!owned) {
                if (auto lock = CCSprite::createWithSpriteFrameName("GJ_lock_001.png")) {
                    lock->setScale(.42f);
                    lock->setPosition({Cell / 2, Cell / 2});
                    cell->addChild(lock, 2);
                }
            }
            auto button = CCMenuItemSpriteExtra::create(cell, this, menu_selector(GeoBRArrowGarage::onArrowSkin));
            button->setTag(skin);
            int column = static_cast<int>(i) % Columns, row = static_cast<int>(i) / Columns;
            button->setPosition(gridCenter + CCPoint{(column - (Columns - 1) / 2.f) * (Cell + 4.f), ((rows - 1) / 2.f - row) * (Cell + 6.f) - 7.f});
            menu->addChild(button);
            if (skin == ArrowSettings::skin()) {
                if (auto cursor = CCSprite::createWithSpriteFrameName("GJ_select_001.png")) {
                    cursor->setID("geobr-arrow-cursor");
                    cursor->setScale(.85f);
                    cursor->setPosition(button->getPosition());
                    page->addChild(cursor, -1);
                }
            }
        }
        auto label = CCLabelBMFont::create("Arrow", "goldFont.fnt");
        label->setScale(.55f);
        label->setPosition(gridCenter + CCPoint{0.f, gridSize.height / 2 - 11.f});
        page->addChild(label, 1);
    }

    void closeArrowPage() {
        auto& f = *m_fields.operator->();
        f.active = false;
        for (auto const& [node, visible] : f.hidden) if (node) node->setVisible(visible);
        f.hidden.clear();
        if (f.page) f.page->removeFromParent();
        f.page = nullptr;
        f.preview = nullptr;
        if (f.tabIcon) f.tabIcon->setAppearance(tabAppearance(false));
    }

    void onArrowSkin(CCObject* sender) {
        int skin = sender ? sender->getTag() : 1;
        auto const& info = skinInfo(skin);
        if (!isOwned(skin)) {
            auto title = fmt::format("Arrow {}", info.name);
            if (info.source == SkinSource::Shop) {
                createQuickPopup(title.c_str(), fmt::format("{}\nCompre na aba <cy>Arrows</c> da loja.", unlockHint(skin)), "OK", "Loja",
                    [](auto, bool shop) { if (shop) if (auto popup = geobr::TierShopPopup::create(3)) popup->show(); });
            }
            else {
                FLAlertLayer::create(title.c_str(), unlockHint(skin), "OK")->show();
            }
            return;
        }
        ArrowSettings::equip(skin);
        // Rebuild so the cursor, preview and tab follow the new skin.
        closeArrowPage();
        openArrowPage();
    }
};
