// RC9 collab editor: the hooks into GD's own screens. Every addition is a
// separate menu with a geobr-* ID; nothing native is moved, hidden or
// re-laid-out, so GD and other mods keep their layouts.
#include "CollabMerge.hpp"
#include "CollabService.hpp"
#include "../ui/CollabPopup.hpp"
#include "../ui/SocialPopups.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/UploadPopup.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/Notification.hpp>

#include <unordered_map>

using namespace geode::prelude;
using namespace geobr::collab;

namespace {

CCNode* findFrame(CCNode* root, char const* frame, int depth = 0) {
    if (!root || depth > 8) return nullptr;
    if (isSpriteFrameName(root, frame)) return root;
    for (auto child : CCArrayExt<CCNode*>(root->getChildren())) {
        if (auto found = findFrame(child, frame, depth + 1)) return found;
    }
    return nullptr;
}

// GD's creator label reads "By <name>".
CCLabelBMFont* findCreatorLabel(CCNode* root, int depth = 0) {
    if (!root || depth > 6) return nullptr;
    if (auto label = typeinfo_cast<CCLabelBMFont*>(root)) {
        std::string text = label->getString();
        if (text.starts_with("By ")) return label;
    }
    for (auto child : CCArrayExt<CCNode*>(root->getChildren())) {
        if (auto found = findCreatorLabel(child, depth + 1)) return found;
    }
    return nullptr;
}

CCPoint worldRightOf(CCNode* node) {
    auto box = node->boundingBox();
    return node->getParent() ? node->getParent()->convertToWorldSpace({box.getMaxX(), box.getMidY()}) : CCPoint{box.getMaxX(), box.getMidY()};
}

std::string roomOfEditor(LevelEditorLayer* editor) {
    return editor ? roomOfSettings(editor->m_levelSettings) : std::string();
}

// Applies a merged level to the open editor without reloading it: objects
// that only exist in `mine` are removed, the ones only in `merged` created.
int applyToEditor(EditorUI* ui, std::string const& mine, std::string const& merged) {
    auto editor = ui ? ui->m_editorLayer : nullptr;
    if (!editor || !editor->m_objects) return 0;
    auto a = splitLevel(mine), b = splitLevel(merged);
    std::unordered_map<std::string, int> count;
    for (auto const& o : b.objects) ++count[o];
    std::unordered_map<std::string, int> remove;
    for (auto const& o : a.objects) {
        auto it = count.find(o);
        if (it != count.end() && it->second > 0) --it->second;
        else ++remove[o];
    }
    std::string add;
    int added = 0;
    for (auto const& [o, n] : count) for (int i = 0; i < n; ++i) { add += o; add += ';'; ++added; }
    ui->deselectAll();
    if (!remove.empty()) {
        std::vector<GameObject*> victims;
        for (auto object : CCArrayExt<GameObject*>(editor->m_objects)) {
            if (!object) continue;
            std::string saved = object->getSaveString(editor);
            auto it = remove.find(saved);
            if (it != remove.end() && it->second > 0) { --it->second; victims.push_back(object); }
        }
        for (auto object : victims) editor->removeObject(object, true);
    }
    if (!add.empty()) editor->createObjectsFromString(gd::string(add), true, true);
    return added;
}

} // namespace

// "My Collabs" next to GD's New button in My Levels (after joining a room).
class $modify(GeoBRCollabBrowser, LevelBrowserLayer) {
    bool init(GJSearchObject* search) {
        if (!LevelBrowserLayer::init(search)) return false;
        if (!search || search->m_searchType != SearchType::MyLevels || !CollabService::get()->hasRooms()) return true;
        auto win = CCDirector::sharedDirector()->getWinSize();
        CCPoint anchor {win.width - 32.f, 32.f};
        if (auto newSprite = findFrame(this, "GJ_newBtn_001.png")) {
            auto item = newSprite->getParent();
            if (item && item->getParent()) anchor = item->getParent()->convertToWorldSpace(item->getPosition());
        }
        auto icon = CCSprite::create("geobr-collab-icon.png"_spr);
        if (!icon) return true;
        icon->setScale(30.f / icon->getContentWidth());
        auto sprite = CircleButtonSprite::create(icon, CircleBaseColor::Blue, CircleBaseSize::Medium);
        sprite->setScale(.8f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(GeoBRCollabBrowser::onMyCollabs));
        button->setID("geobr-my-collabs-button");
        auto menu = CCMenu::create();
        menu->setID("geobr-my-collabs-menu");
        auto pos = convertToNodeSpace(anchor + CCPoint{0.f, 62.f});
        pos.y = std::min(pos.y, win.height - 40.f);
        menu->setPosition(pos);
        menu->addChild(button);
        auto label = CCLabelBMFont::create("My Collabs", "bigFont.fnt");
        label->setScale(.22f);
        label->setPosition({0.f, -24.f});
        menu->addChild(label);
        addChild(menu, 20);
        return true;
    }

    void onMyCollabs(CCObject*) {
        if (auto popup = geobr::CollabPopup::create()) popup->show();
    }
};

// Sync and Chat buttons in the editor of a collab level.
class $modify(GeoBRCollabEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editor) {
        if (!EditorUI::init(editor)) return false;
        if (roomOfEditor(editor).empty()) return true;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("geobr-collab-editor-menu");
        menu->setPosition({win.width / 2.f, win.height - 18.f});
        char const* labels[] = {"Sync", "Chat"};
        SEL_MenuHandler handlers[] = {menu_selector(GeoBRCollabEditorUI::onCollabSync), menu_selector(GeoBRCollabEditorUI::onCollabChat)};
        for (int i = 0; i < 2; ++i) {
            auto sprite = ButtonSprite::create(labels[i], 46, true, "bigFont.fnt", i ? "GJ_button_02.png" : "GJ_button_01.png", 24, .5f);
            sprite->setScale(.75f);
            auto button = CCMenuItemSpriteExtra::create(sprite, this, handlers[i]);
            button->setID(i ? "geobr-collab-chat" : "geobr-collab-sync");
            button->setPosition({i ? 26.f : -26.f, 0.f});
            menu->addChild(button);
        }
        addChild(menu, 100);
        return true;
    }

    void onCollabSync(CCObject*) {
        auto room = roomOfEditor(m_editorLayer);
        if (room.empty()) return;
        std::string mine = m_editorLayer->getLevelString();
        Notification::create("Sincronizando com a sala...", NotificationIcon::Loading, 1.f)->show();
        WeakRef<EditorUI> weak = this;
        CollabService::get()->sync(room, mine, [weak, mine](bool ok, std::string message, std::string merged, bool changed) {
            if (!ok) { Notification::create(message, NotificationIcon::Warning, 2.5f)->show(); return; }
            int added = 0;
            if (changed) if (auto self = weak.lock()) added = applyToEditor(self.data(), mine, merged);
            Notification::create(changed ? fmt::format("{} | {} objeto(s) de outros membros", message, added) : message,
                NotificationIcon::Success, 2.f)->show();
        });
    }

    void onCollabChat(CCObject*) {
        auto room = roomOfEditor(m_editorLayer);
        if (room.empty()) return;
        std::string title = m_editorLayer->m_level ? std::string(m_editorLayer->m_level->m_levelName) : std::string("Collab");
        if (auto popup = geobr::ChatPopup::create(geobr::ChatKind::Collab, room, title)) popup->show();
    }
};

// Saving a collab level also sends it (no merge here: if someone else
// changed the room meanwhile, the editor's Sync merges and shows it).
class $modify(GeoBRCollabPause, EditorPauseLayer) {
    void saveLevel() {
        EditorPauseLayer::saveLevel();
        auto room = roomOfEditor(m_editorLayer);
        if (room.empty() || CollabService::get()->syncing()) return;
        std::string mine = m_editorLayer->getLevelString();
        CollabService::get()->sync(room, mine, [](bool ok, std::string message, std::string, bool) {
            Notification::create(ok ? "Collab: alteracoes enviadas" : message, ok ? NotificationIcon::Success : NotificationIcon::Warning, 2.f)->show();
        }, false);
    }
};

// Uploading a collab level links its GD ID to the room (credits "+").
class $modify(GeoBRCollabUpload, UploadPopup) {
    void levelUploadFinished(GJGameLevel* level) {
        UploadPopup::levelUploadFinished(level);
        if (!level) return;
        auto room = CollabService::roomOf(level);
        if (!room.empty()) CollabService::get()->publish(room, level->m_levelID.value());
    }
};

namespace {
void addCreditsButton(CCNode* host, GJGameLevel* level, CCObject* target, SEL_MenuHandler handler, float scale) {
    if (!host || !level) return;
    if (auto old = host->getChildByID("geobr-collab-credits-menu")) old->removeFromParent();
    if (!CollabService::get()->isPublished(level->m_levelID.value())) return;
    auto plus = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
    if (!plus) return;
    plus->setScale(scale);
    auto button = CCMenuItemSpriteExtra::create(plus, target, handler);
    button->setID("geobr-collab-credits");
    auto menu = CCMenu::create();
    menu->setID("geobr-collab-credits-menu");
    CCPoint world;
    if (auto creator = findCreatorLabel(host)) world = worldRightOf(creator) + CCPoint{12.f, 0.f};
    else world = host->convertToWorldSpace({host->getContentSize().width / 2.f, host->getContentSize().height / 2.f});
    menu->setPosition(host->convertToNodeSpace(world));
    menu->addChild(button);
    host->addChild(menu, 30);
}
} // namespace

class $modify(GeoBRCollabCell, LevelCell) {
    void loadFromLevel(GJGameLevel* level) {
        LevelCell::loadFromLevel(level);
        addCreditsButton(this, level, this, menu_selector(GeoBRCollabCell::onCollabCredits), .32f);
    }

    void onCollabCredits(CCObject*) {
        if (!m_level) return;
        if (auto popup = geobr::CollabCreditsPopup::create(m_level->m_levelID.value(), m_level->m_levelName)) popup->show();
    }
};

class $modify(GeoBRCollabInfo, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        addCreditsButton(this, level, this, menu_selector(GeoBRCollabInfo::onCollabCredits), .42f);
        return true;
    }

    void onCollabCredits(CCObject*) {
        if (!m_level) return;
        if (auto popup = geobr::CollabCreditsPopup::create(m_level->m_levelID.value(), m_level->m_levelName)) popup->show();
    }
};
