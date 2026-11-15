#pragma once

#include "../services/CloudService.hpp"
#include "../models/CommunityMessage.hpp"
#include "../models/PlayerIdentity.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
#include <chrono>
#include <vector>

namespace geobr {

class CommunityTab final : public cocos2d::CCLayer {
public:
    static CommunityTab* create(cocos2d::CCSize size);

private:
    geode::ScrollLayer* m_list = nullptr;
    geode::TextInput* m_input = nullptr;
    CCMenuItemSpriteExtra* m_send = nullptr;
    cocos2d::CCLabelBMFont* m_feedback = nullptr;
    cocos2d::CCNode* m_state = nullptr;
    std::vector<CommunityMessage> m_messages;
    std::chrono::steady_clock::time_point m_last {};
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_fetch;
    CloudRequest m_cloud;
    std::string m_reply;
    void onReply(cocos2d::CCObject*);void onCancelReply(cocos2d::CCObject*);
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_post;

    bool init(cocos2d::CCSize size);
    void onRefresh(cocos2d::CCObject*);void onPlayers(cocos2d::CCObject*);void onProfile(cocos2d::CCObject*);
    void onDM(cocos2d::CCObject*);void onKeyboard(cocos2d::CCObject*);
    void onSend(cocos2d::CCObject*);
    void poll(float);
    void refresh();
    void showState(std::string const&, bool loading, bool error = false);
    void feedback(std::string const&, bool error = false);
    void render(std::vector<CommunityMessage>);
    cocos2d::CCNode* card(CommunityMessage const&);
};

}
