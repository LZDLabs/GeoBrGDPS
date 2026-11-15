#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>
namespace geobr {
enum class AdminEntity { News, Announcements, Seasons, Rewards, Quests, MainLevels, Shop, Ranks, Links };
std::string adminTable(AdminEntity);std::string adminTitle(AdminEntity);
class AdminLogin final:public geode::Popup {
    geode::TextInput* m_password=nullptr;cocos2d::CCLabelBMFont* m_status=nullptr;CCMenuItemSpriteExtra* m_submit=nullptr;
    CloudRequest m_request;bool init()override;void onSubmit(cocos2d::CCObject*);
public:static AdminLogin* create();
};
class AdminPanel final:public geode::Popup {
    CloudRequest m_request;
    geode::ScrollLayer* m_list=nullptr;cocos2d::CCLabelBMFont* m_status=nullptr;
    std::vector<matjson::Value> m_rows;AdminEntity m_entity=AdminEntity::News;int m_page=0;bool m_busy=false;
    bool init()override;void load();void render();void status(std::string const&);
    void onSafety(cocos2d::CCObject*);void onBuilder(cocos2d::CCObject*);void onTab(cocos2d::CCObject*);void onNew(cocos2d::CCObject*);void onEdit(cocos2d::CCObject*);
    void onRemove(cocos2d::CCObject*);void onPage(cocos2d::CCObject*);void onRefresh(cocos2d::CCObject*);
    void onClose(cocos2d::CCObject*)override;
public:static AdminPanel* create();
};
}
