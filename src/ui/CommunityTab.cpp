#include "CommunityTab.hpp"

#include "UIHelpers.hpp"
#include "GeoBRVisuals.hpp"
#include "../services/AccessService.hpp"
#include "DirectMessagesPopup.hpp"
#include "GeoPlayersPopup.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../services/CommunityService.hpp"
#include "../services/PlayerService.hpp"
#include "../utils/TextSanitizer.hpp"

#include <Geode/ui/LoadingSpinner.hpp>
#include <Geode/ui/TextArea.hpp>
#include <fmt/format.h>
#include <charconv>

using namespace geode::prelude;

namespace geobr {

CommunityTab* CommunityTab::create(CCSize size) {
    auto result = new CommunityTab;
    if (result->init(size)) { result->autorelease(); return result; }
    delete result;
    return nullptr;
}

bool CommunityTab::init(CCSize size) {
    if (!CCLayer::init()) return false;
    setContentSize(size);
    ignoreAnchorPointForPosition(false);
    setAnchorPoint({.5f, .5f});
    setID("community-tab");

    auto title = ui::makeLabel("Chat", "goldFont.fnt", .48f);
    title->setAnchorPoint({0.f, .5f});
    title->setPosition({8.f, size.height - 15.f});
    addChild(title);

    auto refreshMenu = CCMenu::create();
    refreshMenu->setPosition({size.width - 42.f, size.height - 15.f});
    auto refreshButton = ui::makeTextButton(
        "Atualizar", 76, "GJ_button_01.png", this,
        menu_selector(CommunityTab::onRefresh), .38f, 25.f
    );
    refreshButton->setID("community-refresh-button");
    refreshMenu->addChild(refreshButton);
    addChild(refreshMenu);
    auto cancelMenu=CCMenu::create();cancelMenu->setPosition({size.width-144,size.height-15});cancelMenu->addChild(ui::makeTextButton("Cancelar resposta",112,"GJ_button_04.png",this,menu_selector(CommunityTab::onCancelReply),.23f,25));addChild(cancelMenu);
    CCSize listSize {size.width - 12.f, size.height - 86.f};
    auto background = ui::makePanel(listSize, {10, 28, 62}, 170);
    background->setPosition({size.width / 2.f, listSize.height / 2.f + 46.f});
    addChild(background);
    m_list = ScrollLayer::create(listSize, true, true);
    m_list->setPosition({6.f, 46.f});
    m_list->setID("community-scroll");
    m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(3.f));
    addChild(m_list);

    m_feedback = ui::makeLabel("", "chatFont.fnt", .5f, {180, 225, 255});
    m_feedback->setAnchorPoint({0.f, .5f});
    m_feedback->setPosition({7.f, 36.f});
    addChild(m_feedback);

    float inputWidth = size.width - 178.f;
    m_input = TextInput::create(inputWidth, "Escreva uma mensagem...", "chatFont.fnt");
    m_input->setPosition({inputWidth / 2.f + 6.f, 15.f});
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setMaxCharCount(GeoBRConfig::MaxMessageCodepoints);
    m_input->setTextAlign(TextInputAlign::Left);
    m_input->setID("community-message-input");
    m_input->setEnabled(true);
    m_input->getInputNode()->setTouchEnabled(true);
    addChild(m_input, 8);

    auto keyboardMenu = CCMenu::create();
    keyboardMenu->setPosition({inputWidth + 35.f, 15.f});
    auto keyboard = ui::makeTextButton(
        "ABC", 48, "GJ_button_04.png", this,
        menu_selector(CommunityTab::onKeyboard), .38f, 30.f
    );
    keyboard->setID("community-keyboard-button");
    keyboardMenu->addChild(keyboard);
    addChild(keyboardMenu, 10);

    auto sendMenu = CCMenu::create();
    sendMenu->setPosition({size.width - 42.f, 15.f});
    m_send = ui::makeTextButton(
        "Enviar", 76, "GJ_button_01.png", this,
        menu_selector(CommunityTab::onSend), .39f, 30.f
    );
    m_send->setID("community-send-button");
    sendMenu->addChild(m_send);
    addChild(sendMenu, 10);
    refresh();
    schedule(schedule_selector(CommunityTab::poll),3.f);
    return true;
}

void CommunityTab::onDM(CCObject*){if(auto p=DirectMessagesPopup::create())p->show();}
void CommunityTab::onPlayers(CCObject*){if(auto p=GeoPlayersPopup::create())p->show();}

void CommunityTab::onRefresh(CCObject*) { refresh(); }
void CommunityTab::onProfile(CCObject* sender){auto peer=static_cast<CCNode*>(sender)->getID();if(peer.empty()||peer==CloudService::userID()){feedback("Este e o seu perfil.");return;}if(auto p=DirectMessagesPopup::create(peer))p->show();}

void CommunityTab::onKeyboard(CCObject*) {
    m_input->setEnabled(true);
    m_input->getInputNode()->setTouchEnabled(true);
    m_input->focus();
    feedback("Teclado aberto. Digite sua mensagem.");
}

void CommunityTab::refresh() {
    m_fetch.cancel();
    feedback("");
    if (auto error = GeoBRConfig::supabaseConfigurationError(); !error.empty()) {
        showState(error, false, true);
        return;
    }
    showState("Atualizando o Chat Geral...", true);
    m_fetch.spawn("GeoBR - Comunidade Supabase", CommunityService::fetchMessages(), [this](auto response) {
        auto result = CommunityService::parseMessages(response);
        if (!result.success) showState(result.error, false, true);
        else render(std::move(result.data));
    });
}

void CommunityTab::onReply(CCObject* sender){m_reply=static_cast<CCNode*>(sender)->getID();feedback("Respondendo a #"+m_reply+" | Cancelar resposta no topo.");m_input->focus();}
void CommunityTab::onCancelReply(CCObject*){m_reply.clear();feedback("");}
void CommunityTab::onSend(CCObject*) {
    if(!AccessService::get()->canChat()){feedback(AccessService::get()->reason(),true);return;}
    auto player = PlayerService::current();
    auto message = text::sanitizeMessage(
        m_input->getString().c_str(), GeoBRConfig::MaxMessageCodepoints
    );
    if (message.empty()) { feedback("Digite uma mensagem antes de enviar.", true); return; }
    auto now = std::chrono::steady_clock::now();
    if (m_last.time_since_epoch().count() != 0 && now - m_last < GeoBRConfig::MessageCooldown) {
        feedback("Aguarde alguns segundos antes de enviar novamente.", true);
        return;
    }
    if (auto error = GeoBRConfig::supabaseConfigurationError(); !error.empty()) {
        feedback(error, true);
        return;
    }
    m_last = now;
    m_send->setEnabled(false);
    feedback("Enviando...");
    auto body=matjson::makeObject({{"username",player.username.empty()?"Jogador GeoBR":player.username},{"message",message},{"account_id",player.accountId},{"player_id",player.playerId}});
    if(!m_reply.empty()){long long replyID=0;auto parsed=std::from_chars(m_reply.data(),m_reply.data()+m_reply.size(),replyID);if(parsed.ec==std::errc{}&&replyID>0)body["reply_to"]=replyID;else m_reply.clear();}
    m_cloud.call("POST","/rest/v1/geobr_community_messages",body,[this](auto result){
            m_send->setEnabled(true);
            if(!result.success){feedback(result.error,true);return;}
            m_reply.clear();m_input->setString("");refresh();
        });
}
void CommunityTab::poll(float){
    if(!isVisible()||m_fetch.isPending())return;
    m_fetch.spawn("GeoBR - atualizar chat",CommunityService::fetchMessages(),[this](auto response){
        auto result=CommunityService::parseMessages(response);
        if(result.success){
            if(result.data.size()==m_messages.size()&&!result.data.empty()&&result.data.front().id==m_messages.front().id)return;
            render(std::move(result.data));
        }
    });
}

void CommunityTab::showState(std::string const& text, bool loading, bool error) {
    if (m_state) m_state->removeFromParent();
    m_list->setVisible(false);
    m_state = CCNode::create();
    m_state->setContentSize(m_list->getContentSize());
    m_state->setAnchorPoint({.5f, .5f});
    m_state->setPosition(m_list->getPosition() + m_list->getContentSize() / 2.f);
    auto label = ui::makeLabel(text, "bigFont.fnt", .4f, error ? ccColor3B{255,150,135} : ccColor3B{255,255,255});
    label->limitLabelWidth(m_list->getContentWidth() - 50.f, .4f, .24f);
    label->setPosition({m_state->getContentWidth() / 2.f, m_state->getContentHeight() / 2.f - (loading ? 18.f : 0.f)});
    m_state->addChild(label);
    if (loading) {
        auto spinner = LoadingSpinner::create(30.f);
        spinner->setPosition({m_state->getContentWidth() / 2.f, m_state->getContentHeight() / 2.f + 18.f});
        m_state->addChild(spinner);
    }
    addChild(m_state, 5);
}

void CommunityTab::feedback(std::string const& text, bool error) {
    m_feedback->setString(text.c_str());
    m_feedback->setColor(error ? ccColor3B{255,145,130} : ccColor3B{140,255,158});
    m_feedback->limitLabelWidth(getContentWidth() - 15.f, .5f, .28f);
}

void CommunityTab::render(std::vector<CommunityMessage> messages) {
    m_messages = std::move(messages);
    if (m_state) { m_state->removeFromParent(); m_state = nullptr; }
    if (m_messages.empty()) { showState("O Chat Geral ainda esta vazio. Seja o primeiro!", false); return; }
    m_list->setVisible(true);
    ui::clearScroll(m_list);
    // Background refreshes keep the reader's position unless already at the top.
    auto layer = m_list->m_contentLayer;
    float fromTop = layer->getContentHeight() + layer->getPositionY() - m_list->getContentHeight();
    bool atTop = fromTop <= 4.f;
    for (auto const& message : m_messages) layer->addChild(card(message));
    layer->updateLayout();
    if (atTop) m_list->scrollToTop();
    else layer->setPositionY(m_list->getContentHeight() - layer->getContentHeight() + fromTop);
}

CCNode* CommunityTab::card(CommunityMessage const& message) {
 bool mine=message.userID==CloudService::userID();float height=message.replyTo.empty()?78.f:91.f;float width=m_list->getContentWidth()-8;
 auto card=CCNode::create();card->setContentSize({width,height});card->setAnchorPoint({.5f,.5f});auto background=ui::makePanel({width,height},mine?ccColor3B{43,43,80}:ccColor3B{23,39,66},245);background->setPosition({width/2,height/2});card->addChild(background);
 auto avatar=visual::playerIcon(message.avatar,IconType::Cube,23);auto avatarButton=CCMenuItemSpriteExtra::create(avatar,this,menu_selector(CommunityTab::onProfile));avatarButton->setID(message.userID);auto avatarMenu=CCMenu::create();avatarMenu->setPosition({22,height-19});avatarMenu->addChild(avatarButton);card->addChild(avatarMenu);
 auto username=ui::makeLabel(message.username+(mine?" (voce)":""),"bigFont.fnt",.31f,{255,222,120});username->setAnchorPoint({0,.5f});username->setPosition({42,height-13});username->limitLabelWidth(width-160,.31f,.22f);card->addChild(username);
 auto stamp=message.createdAt;stamp=stamp.size()>=16?stamp.substr(11,5)+" UTC":"";auto date=ui::makeLabel(stamp,"chatFont.fnt",.36f,{155,191,220});date->setAnchorPoint({1,.5f});date->setPosition({width-10,height-13});card->addChild(date);
 float y=height-28;if(!message.replyTo.empty()){auto quote=ui::makeLabel("> "+(message.replyBody.empty()?"Mensagem #"+message.replyTo:message.replyBody),"chatFont.fnt",.39f,{145,208,219});quote->setAnchorPoint({0,.5f});quote->setPosition({42,y});quote->limitLabelWidth(width-55,.39f,.25f);card->addChild(quote);y-=12;}
 auto body=SimpleTextArea::create(message.message,"chatFont.fnt",.51f,width-55);body->setAnchorPoint({0,1});body->setAlignment(kCCTextAlignmentLeft);body->setWrappingMode(WrappingMode::WORD_WRAP);body->setMaxLines(4);body->setPosition({42,y});card->addChild(body);
 auto replyMenu=CCMenu::create();replyMenu->setPosition({width-44,12});auto reply=ui::makeTextButton("Responder",73,"GJ_button_04.png",this,menu_selector(CommunityTab::onReply),.21f,18);reply->setID(message.id);replyMenu->addChild(reply);card->addChild(replyMenu);
 return card;
}
}
