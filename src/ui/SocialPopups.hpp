#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <functional>

// RC9 social screens: chat for groups and collab rooms, member lists, a
// player picker, the player card (full icon set) and groups.
namespace geobr {

enum class ChatKind { Group, Collab };

class ChatPopup final : public geode::Popup {
public:
    static ChatPopup* create(ChatKind kind, std::string id, std::string title);
private:
    ChatKind m_kind = ChatKind::Group;
    std::string m_id, m_requestID, m_pendingBody;
    CloudRequest m_fetch, m_send;
    geode::ScrollLayer* m_list = nullptr;
    geode::TextInput* m_input = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    CCMenuItemSpriteExtra* m_submit = nullptr;
    matjson::Value m_rows;
    bool m_busy = false, m_fetchPending = false;
    bool init(ChatKind kind, std::string id, std::string title);
    void fetch(float = 0);
    void render();
    void onSend(cocos2d::CCObject*);
};

class MembersPopup final : public geode::Popup {
public:
    static MembersPopup* create(ChatKind kind, std::string id, std::string title);
private:
    ChatKind m_kind = ChatKind::Group;
    std::string m_id;
    CloudRequest m_fetch, m_action;
    cocos2d::CCNode* m_view = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    matjson::Value m_rows;
    int m_page = 0, m_rank = 1;
    bool m_owner = false;
    bool init(ChatKind kind, std::string id, std::string title);
    void render();
    void onPage(cocos2d::CCObject*);
    void onActions(cocos2d::CCObject*);
    void fetch();
    void onPlayer(cocos2d::CCObject*);
    void onRemove(cocos2d::CCObject*);
    void onToggleAdmin(cocos2d::CCObject*);
    void onAdd(cocos2d::CCObject*);
};

class PlayerPickerPopup final : public geode::Popup {
public:
    using Pick = std::function<void(matjson::Value)>;
    static PlayerPickerPopup* create(std::string title, Pick pick);
private:
    Pick m_pick;
    CloudRequest m_request;
    geode::TextInput* m_input = nullptr;
    cocos2d::CCNode* m_view = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    matjson::Value m_data;
    int m_page = 0;
    bool m_busy = false;
    bool init(std::string title, Pick pick);
    void fetch();
    void render();
    void onSearch(cocos2d::CCObject*);
    void onPage(cocos2d::CCObject*);
    void onPick(cocos2d::CCObject*);
};

class PlayerCardPopup final : public geode::Popup {
public:
    static PlayerCardPopup* create(matjson::Value player);
private:
    matjson::Value m_player;
    bool init(matjson::Value player);
    void onMessage(cocos2d::CCObject*);
    void onGroup(cocos2d::CCObject*);
};

class GroupsPopup final : public geode::Popup {
public:
    // With `addUser`, picking a group adds that player to it.
    static GroupsPopup* create(std::string addUser = "", std::string addName = "");
private:
    std::string m_addUser, m_addName;
    CloudRequest m_fetch, m_action;
    cocos2d::CCNode* m_view = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    geode::TextInput* m_input = nullptr;
    matjson::Value m_groups;
    matjson::Value m_selected;
    int m_page = 0;
    bool m_busy = false;
    bool init(std::string addUser, std::string addName);
    void fetch();
    void render();
    void onPage(cocos2d::CCObject*);
    void onGroup(cocos2d::CCObject*);
    void onCreate(cocos2d::CCObject*);
    void onBack(cocos2d::CCObject*);
    void onChat(cocos2d::CCObject*);
    void onMembers(cocos2d::CCObject*);
    void onLeave(cocos2d::CCObject*);
};

} // namespace geobr
