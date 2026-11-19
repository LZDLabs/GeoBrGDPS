// RC9 collab editor: the hooks into GD's own screens. Every addition is a
// separate menu with a geobr-* ID; nothing native is moved, hidden or
// re-laid-out, so GD and other mods keep their layouts.
#include "CollabLive.hpp"
#include "CollabMerge.hpp"
#include "CollabService.hpp"
#include "../ui/CollabPopup.hpp"
#include "../ui/SocialPopups.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCTextInputNode.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelSettingsLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
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
        auto room = roomOfEditor(editor);
        if (room.empty()) return true;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("geobr-collab-editor-menu");
        menu->setPosition({win.width / 2.f, win.height - 18.f});
        auto sprite = ButtonSprite::create("Chat", 46, true, "bigFont.fnt", "GJ_button_02.png", 24, .5f);
        sprite->setScale(.75f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(GeoBRCollabEditorUI::onCollabChat));
        button->setID("geobr-collab-chat");
        menu->addChild(button);
        // Members (the host gives / takes ADM there).
        auto membersSprite = ButtonSprite::create("Membros", 70, true, "bigFont.fnt", "GJ_button_04.png", 24, .5f);
        membersSprite->setScale(.75f);
        auto members = CCMenuItemSpriteExtra::create(membersSprite, this, menu_selector(GeoBRCollabEditorUI::onCollabMembers));
        members->setID("geobr-collab-members");
        menu->addChild(members);
        menu->setLayout(RowLayout::create()->setGap(6.f));
        addChild(menu, 100);
        // Live editing: changes and everyone's position, about once per second.
        geobr::collab::startLive(this, room);
        return true;
    }

    void onCollabMembers(CCObject*) {
        auto room = roomOfEditor(m_editorLayer);
        if (room.empty()) return;
        if (auto popup = geobr::MembersPopup::create(geobr::ChatKind::Collab, room, "Membros do collab")) popup->show();
    }

    void onCollabChat(CCObject*) {
        auto room = roomOfEditor(m_editorLayer);
        if (room.empty()) return;
        std::string title = m_editorLayer->m_level ? std::string(m_editorLayer->m_level->m_levelName) : std::string("Collab");
        if (auto popup = geobr::ChatPopup::create(geobr::ChatKind::Collab, room, title)) popup->show();
    }
};

// Level settings of a collab level: only the host and the ADMs change them
// (they go to everyone live). For the others everything is grey and disabled
// except OK; any change would be put back to the room's settings anyway.
class $modify(GeoBRCollabLevelSettings, LevelSettingsLayer) {
    bool init(LevelSettingsObject* settings, LevelEditorLayer* editor) {
        if (!LevelSettingsLayer::init(settings, editor)) return false;
        auto room = editor ? roomOfEditor(editor) : std::string();
        if (room.empty() || geobr::collab::canSaveRoom(room)) return true;
        lockDown(m_mainLayer);
        auto size = m_mainLayer->getContentSize();
        auto note = CCLabelBMFont::create("Somente o host e os ADMs alteram as configuracoes", "bigFont.fnt");
        note->setScale(.32f);
        note->setColor({255, 120, 120});
        note->setPosition({size.width / 2.f, size.height - 12.f});
        note->setID("geobr-collab-settings-locked");
        m_mainLayer->addChild(note, 100);
        return true;
    }

    // The OK button (ButtonSprite "OK"; "ok-button" with node-ids) stays usable.
    static bool isOk(CCMenuItem* item) {
        if (item->getID() == "ok-button") return true;
        auto sprite = typeinfo_cast<CCMenuItemSpriteExtra*>(item);
        auto button = sprite ? typeinfo_cast<ButtonSprite*>(sprite->getNormalImage()) : nullptr;
        return button && button->m_label && std::string(button->m_label->getString()) == "OK";
    }

    void lockDown(CCNode* node) {
        if (!node) return;
        for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
            if (auto item = typeinfo_cast<CCMenuItem*>(child)) {
                if (isOk(item)) continue;
                item->setEnabled(false);
                if (auto rgba = typeinfo_cast<CCRGBAProtocol*>(item)) rgba->setColor(ccColor3B{110, 110, 110});
                continue;
            }
            if (auto input = typeinfo_cast<CCTextInputNode*>(child)) { input->setTouchEnabled(false); continue; }
            lockDown(child);
        }
    }
};

// After a crash in the live collab editor, its trace goes to the server once.
class $modify(GeoBRCollabReportMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        geobr::collab::uploadPendingTrace();
        return true;
    }
};

// Saving a collab level: the host and the ADMs save the room's level; the
// others' edits are already in the room (live), so nothing is overwritten.
class $modify(GeoBRCollabPause, EditorPauseLayer) {
    void saveLevel() {
        EditorPauseLayer::saveLevel();
        auto room = roomOfEditor(m_editorLayer);
        if (room.empty()) return;
        if (!geobr::collab::canSaveRoom(room)) {
            Notification::create("Collab: so o host e os ADMs salvam o level. Suas edicoes ja estao na sala.", NotificationIcon::Info, 3.f)->show();
            return;
        }
        std::string level = m_editorLayer->getLevelString();
        CollabService::get()->saveSnapshot(room, level, [](bool ok, std::string message) {
            Notification::create(ok ? "Collab: level da sala salvo" : message, ok ? NotificationIcon::Success : NotificationIcon::Warning, 2.f)->show();
        });
    }
};

// Publishing a collab level: host and ADMs only.
class $modify(GeoBRCollabShare, EditLevelLayer) {
    void onShare(CCObject* sender) {
        auto room = m_level ? CollabService::roomOf(m_level) : std::string();
        if (!room.empty() && !geobr::collab::canSaveRoom(room)) {
            FLAlertLayer::create("Collab", "So o <cy>host</c> e os <cg>ADMs</c> da sala podem publicar este level.", "OK")->show();
            return;
        }
        EditLevelLayer::onShare(sender);
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
