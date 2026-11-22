#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <functional>

class EditorUI;

// 1.17 collab moderation: the room panel (clean, undo the room, reload,
// lock, new code, restore, bans, log) and the actions on one member (revert
// their edits, kick, ban, mute, view only, ADM).
namespace geobr {

class CollabRoomPopup final : public geode::Popup {
public:
    static CollabRoomPopup* create(std::string room, EditorUI* editor);
private:
    enum class View { Main, Bans, Log };
    View m_view = View::Main;
    std::string m_room;
    geode::WeakRef<EditorUI> m_editor;
    CloudRequest m_fetch, m_action, m_pull;
    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    matjson::Value m_info;
    bool m_busy = false;
    int m_banPage = 0;
    bool init(std::string room, EditorUI* editor);
    void fetch();
    void render();
    void renderMain(cocos2d::CCMenu*);
    void renderBans(cocos2d::CCMenu*);
    void renderLog();
    int rank() const;
    void setStatus(std::string const&);
    void run(std::string fn, matjson::Value args, std::string done, bool refetch = true);
    void confirm(std::string title, std::string text, std::string yes, std::function<void()> action);
    void onYellow(cocos2d::CCObject*);
    void onDupes(cocos2d::CCObject*);
    void onUndoRoom(cocos2d::CCObject*);
    void onReloadAll(cocos2d::CCObject*);
    void onReloadMe(cocos2d::CCObject*);
    void onLock(cocos2d::CCObject*);
    void onNewCode(cocos2d::CCObject*);
    void onCopyCode(cocos2d::CCObject*);
    void onRestoreSave(cocos2d::CCObject*);
    void onClearAll(cocos2d::CCObject*);
    void onShowBans(cocos2d::CCObject*);
    void onShowLog(cocos2d::CCObject*);
    void onBack(cocos2d::CCObject*);
    void onUnban(cocos2d::CCObject*);
    void onBanPage(cocos2d::CCObject*);
};

class CollabMemberPopup final : public geode::Popup {
public:
    using Done = std::function<void()>;
    static CollabMemberPopup* create(std::string room, matjson::Value member, int myRank, Done done);
private:
    std::string m_room, m_user, m_name;
    matjson::Value m_member;
    int m_myRank = 0;
    Done m_done;
    CloudRequest m_action;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    bool m_busy = false;
    bool init(std::string room, matjson::Value member, int myRank, Done done);
    void run(std::string fn, matjson::Value args, std::string done, bool close);
    void confirm(std::string title, std::string text, std::string yes, std::function<void()> action);
    void onRevert(cocos2d::CCObject*);
    void onKick(cocos2d::CCObject*);
    void onBan(cocos2d::CCObject*);
    void onMute(cocos2d::CCObject*);
    void onViewer(cocos2d::CCObject*);
    void onAdmin(cocos2d::CCObject*);
    void onProfile(cocos2d::CCObject*);
};

} // namespace geobr
