#pragma once
#include <ctime>
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

// RC9 collab editor: rooms, codes, opening the shared level, chat, members.
namespace geobr {

class CollabPopup final : public geode::Popup {
public:
    static CollabPopup* create();
private:
    enum class View { List, Detail, Create };
    View m_view = View::List;
    CloudRequest m_fetch, m_action, m_join;
    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    geode::TextInput* m_code = nullptr;
    geode::TextInput* m_title = nullptr;
    matjson::Value m_rooms;
    matjson::Value m_selected;
    int m_page = 0, m_levelPage = 0, m_source = -1;
    bool m_busy = false;      // create / open (one at a time)
    bool m_joining = false;
    std::time_t m_busySince = 0, m_joinSince = 0;
    bool startAction(bool& flag, std::time_t& since, char const* message);
    bool init();
    void fetch();
    void render();
    void renderList(cocos2d::CCMenu*);
    void renderDetail(cocos2d::CCMenu*);
    void renderCreate(cocos2d::CCMenu*);
    void onPage(cocos2d::CCObject*);
    void onRoom(cocos2d::CCObject*);
    void onJoin(cocos2d::CCObject*);
    void onNew(cocos2d::CCObject*);
    void onSource(cocos2d::CCObject*);
    void onLevelPage(cocos2d::CCObject*);
    void onCreate(cocos2d::CCObject*);
    void onBack(cocos2d::CCObject*);
    void onCopy(cocos2d::CCObject*);
    void onOpen(cocos2d::CCObject*);
    void onChat(cocos2d::CCObject*);
    void onMembers(cocos2d::CCObject*);
    void onInvite(cocos2d::CCObject*);
    void onLeave(cocos2d::CCObject*);
};

class CollabCreditsPopup final : public geode::Popup {
public:
    static CollabCreditsPopup* create(int levelID, std::string levelName);
private:
    CloudRequest m_fetch;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    matjson::Value m_members;
    bool init(int levelID, std::string levelName);
    void onPlayer(cocos2d::CCObject*);
};

} // namespace geobr
