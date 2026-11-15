#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
namespace geobr {
class DirectMessagesPopup final:public geode::Popup {
 CloudRequest m_fetch,m_send,m_action;std::string m_peer,m_reply,m_requestID,m_peerName,m_pendingBody;
 geode::ScrollLayer* m_list=nullptr;geode::TextInput* m_input=nullptr;
 cocos2d::CCLabelBMFont* m_status=nullptr;CCMenuItemSpriteExtra* m_submit=nullptr;CCMenuItemSpriteExtra* m_block=nullptr;cocos2d::CCLabelBMFont* m_context=nullptr;
 bool m_busy=false,m_fetchPending=false;matjson::Value m_rows;
 bool init(std::string const&);void fetch(float=0);void render();void onPeer(cocos2d::CCObject*);
 void onPlayers(cocos2d::CCObject*);void onCancel(cocos2d::CCObject*);void onSend(cocos2d::CCObject*);void onReply(cocos2d::CCObject*);void onInbox(cocos2d::CCObject*);void onBlock(cocos2d::CCObject*);
public:static DirectMessagesPopup* create(std::string const& peer="");
};
}
