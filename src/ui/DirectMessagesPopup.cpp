#include "DirectMessagesPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "GeoPlayersPopup.hpp"
#include "UIHelpers.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../utils/TextSanitizer.hpp"
#include "../services/AccessService.hpp"
#include <Geode/ui/TextArea.hpp>
using namespace geode::prelude;
namespace geobr {
DirectMessagesPopup* DirectMessagesPopup::create(std::string const& peer){auto p=new DirectMessagesPopup;if(p->init(peer)){p->autorelease();return p;}delete p;return nullptr;}
bool DirectMessagesPopup::init(std::string const& peer){
 if(!Popup::init(510,310))return false;visual::fitPopup(m_mainLayer,{510,310});visual::stylePopup(m_mainLayer,m_bgSprite,{510,310},{31,30,55});setTitle("Mensagens privadas");setID("geobr-dms");m_peer=peer;
 auto panel=ui::makePanel({476,182},{14,24,43},255);panel->setPosition({255,160});m_mainLayer->addChild(panel);
 m_list=ScrollLayer::create({468,176},true,true);m_list->setPosition({21,72});m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(6));m_mainLayer->addChild(m_list);
 m_context=ui::makeLabel("","bigFont.fnt",.32f,{164,231,225});m_context->setPosition({255,269});m_mainLayer->addChild(m_context);
 m_input=TextInput::create(335,"Escreva uma mensagem...","chatFont.fnt");m_input->setCommonFilter(CommonFilter::Any);m_input->setMaxCharCount(300);m_input->setPosition({188,39});m_mainLayer->addChild(m_input);
 auto menu=CCMenu::create();menu->setPosition({0,0});m_mainLayer->addChild(menu);
 m_submit=ui::makeTextButton("Enviar",105,"GJ_button_01.png",this,menu_selector(DirectMessagesPopup::onSend),.33f,31);m_submit->setPosition({432,39});menu->addChild(m_submit);
 auto inbox=ui::makeTextButton("Voltar",68,"GJ_button_04.png",this,menu_selector(DirectMessagesPopup::onInbox),.26f,24);inbox->setPosition({58,270});menu->addChild(inbox);
 m_block=ui::makeTextButton("Bloquear",83,"GJ_button_06.png",this,menu_selector(DirectMessagesPopup::onBlock),.25f,24);m_block->setPosition({445,270});menu->addChild(m_block);
 auto cancel=ui::makeTextButton("X",24,"GJ_button_04.png",this,menu_selector(DirectMessagesPopup::onCancel),.24f,18);cancel->setPosition({478,62});menu->addChild(cancel);
 auto players=ui::makeTextButton("Nova conversa",150,"GJ_button_01.png",this,menu_selector(DirectMessagesPopup::onPlayers),.31f,29);players->setID("dm-new-conversation");players->setPosition({255,39});menu->addChild(players);
 m_status=ui::makeLabel("Carregando...","chatFont.fnt",.38f,{174,209,237});m_status->setPosition({255,13});m_mainLayer->addChild(m_status);render();fetch();schedule(schedule_selector(DirectMessagesPopup::fetch),3);return true;
}
void DirectMessagesPopup::fetch(float){if(m_fetchPending||!isVisible())return;m_fetchPending=true;auto peer=m_peer;m_fetch.rpc(peer.empty()?"geobr_dm_inbox":"geobr_dm_thread",peer.empty()?matjson::Value::object():matjson::makeObject({{"p_peer",peer}}),[this,peer](auto r){m_fetchPending=false;if(peer!=m_peer)return;if(!r.success){m_status->setString(r.error.c_str());m_status->limitLabelWidth(460,.38f,.22f);return;}if(r.value==m_rows)return;m_rows=r.value;render();});}
void DirectMessagesPopup::render(){
 ui::clearScroll(m_list);bool inbox=m_peer.empty();m_input->setVisible(!inbox);m_submit->setVisible(!inbox);m_block->setVisible(!inbox);if(auto n=m_mainLayer->getChildByIDRecursive("dm-new-conversation"))n->setVisible(inbox);
 m_context->setString(inbox?"Suas conversas":m_peerName.empty()?"Conversa privada":m_peerName.c_str());m_context->limitLabelWidth(272,.32f,.22f);
 if(!m_rows.isArray())return;auto rows=m_rows.asArray().unwrap();
 if(rows.empty()){m_status->setString(inbox?"Abra Nova conversa para escolher um jogador.":"Digite abaixo para enviar a primeira mensagem.");auto hint=ui::makeLabel(inbox?"Comece uma conversa em Jogadores":"Diga oi para iniciar a conversa!","bigFont.fnt",.38f,{174,203,233});hint->limitLabelWidth(410,.38f,.24f);hint->setPosition({234,87});m_list->m_contentLayer->addChild(hint);return;}
 for(auto const& row:rows){bool mine=jsonutil::stringOr(row,"sender")==CloudService::userID();float height=inbox?63.f:jsonutil::stringOr(row,"reply_body").empty()?88.f:101.f;
 auto node=CCNode::create();node->setContentSize({452,height});node->setAnchorPoint({.5f,.5f});auto panel=ui::makePanel({inbox?452.f:410.f,height},mine?ccColor3B{49,42,85}:ccColor3B{25,43,65},255);panel->setPosition({inbox?226.f:mine?247.f:205.f,height/2});node->addChild(panel);
 auto avatar=visual::avatar(row,25);avatar->setPosition({22,height-21});panel->addChild(avatar);
 auto name=ui::makeLabel(inbox?jsonutil::stringOr(row,"name","Jogador"):mine?"Voce":jsonutil::stringOr(row,"sender_name","Jogador"),"bigFont.fnt",.31f,{255,222,134});name->setAnchorPoint({0,.5f});name->setPosition({42,height-14});name->limitLabelWidth(280,.31f,.2f);panel->addChild(name);
 auto stamp=jsonutil::stringOr(row,"created_at");auto time=ui::makeLabel(stamp.size()>15?stamp.substr(11,5):"","chatFont.fnt",.34f,{155,189,216});time->setPosition({panel->getContentWidth()-26,height-14});panel->addChild(time);
 auto quote=jsonutil::stringOr(row,"reply_body");float y=height-29;if(!quote.empty()){auto l=ui::makeLabel("> "+quote,"chatFont.fnt",.38f,{143,208,219});l->setAnchorPoint({0,.5f});l->setPosition({42,y});l->limitLabelWidth(354,.38f,.24f);panel->addChild(l);y-=13;}
 auto body=SimpleTextArea::create(jsonutil::stringOr(row,"body"),"chatFont.fnt",.50f,panel->getContentWidth()-54);body->setAnchorPoint({0,1});body->setAlignment(kCCTextAlignmentLeft);body->setWrappingMode(WrappingMode::WORD_WRAP);body->setMaxLines(inbox?2:5);body->setPosition({42,y});panel->addChild(body);
 if(inbox&&jsonutil::integerOr(row,"unread")>0){auto badge=ui::makeLabel(std::to_string(jsonutil::integerOr(row,"unread"))+" nova(s)","bigFont.fnt",.22f,{120,247,177});badge->setPosition({383,12});panel->addChild(badge);}
 // Whole bubble responds to a tap; its visual stays aligned with the bubble.
 auto hit=CCNode::create();hit->setContentSize({452,height});auto button=CCMenuItemSpriteExtra::create(hit,this,inbox?menu_selector(DirectMessagesPopup::onPeer):menu_selector(DirectMessagesPopup::onReply));button->setID(jsonutil::stringOr(row,inbox?"peer":"id"));auto menu=CCMenu::create();menu->setPosition({226,height/2});menu->addChild(button);node->addChild(menu,4);m_list->m_contentLayer->addChild(node);
 }
 m_list->m_contentLayer->updateLayout();m_list->scrollToTop();if(m_reply.empty())m_status->setString(inbox?"Mensagens recentes e conversas nao lidas":"Toque numa mensagem para responder | X cancela a resposta");
}
void DirectMessagesPopup::onPlayers(CCObject*){if(auto p=GeoPlayersPopup::create())p->show();}
void DirectMessagesPopup::onCancel(CCObject*){m_reply.clear();m_status->setString("Resposta cancelada.");}
void DirectMessagesPopup::onPeer(CCObject* s){if(m_busy)return;m_requestID.clear();m_pendingBody.clear();m_peer=static_cast<CCNode*>(s)->getID();if(m_rows.isArray())for(auto const& r:m_rows.asArray().unwrap())if(jsonutil::stringOr(r,"peer")==m_peer)m_peerName=jsonutil::stringOr(r,"name");m_reply.clear();m_rows=nullptr;render();fetch();}
void DirectMessagesPopup::onReply(CCObject* s){m_reply=static_cast<CCNode*>(s)->getID();m_status->setString("Respondendo a mensagem selecionada | X para cancelar");m_input->focus();}
void DirectMessagesPopup::onInbox(CCObject*){if(m_busy)return;m_requestID.clear();m_pendingBody.clear();m_peer.clear();m_peerName.clear();m_reply.clear();m_rows=nullptr;render();fetch();}
void DirectMessagesPopup::onSend(CCObject*){
 if(m_busy||m_peer.empty())return;if(!AccessService::get()->canDM()){m_status->setString(AccessService::get()->reason().c_str());m_status->limitLabelWidth(460,.38f,.22f);return;}auto body=text::sanitizeMessage(m_input->getString().c_str(),300);if(body.empty())return;if(m_requestID.empty()||m_pendingBody!=m_peer+"\n"+body){m_requestID=visual::uuid();m_pendingBody=m_peer+"\n"+body;}m_busy=true;m_submit->setEnabled(false);m_input->setEnabled(false);m_status->setString("Enviando...");
 auto args=matjson::makeObject({{"p_peer",m_peer},{"p_body",body},{"p_id",m_requestID},{"p_reply",m_reply.empty()?matjson::Value(nullptr):matjson::Value(m_reply)}});m_send.rpc("geobr_dm_send",args,[this](auto r){m_busy=false;m_submit->setEnabled(true);if(!r.success){if(r.httpStatus>=400&&r.httpStatus<500&&r.httpStatus!=401&&r.httpStatus!=429){m_requestID.clear();m_pendingBody.clear();}m_input->setEnabled(true);m_status->setString(r.error.c_str());m_status->limitLabelWidth(460,.38f,.22f);return;}m_input->setEnabled(true);m_requestID.clear();m_pendingBody.clear();m_reply.clear();m_input->setString("");m_status->setString("Mensagem enviada.");fetch();});
}
void DirectMessagesPopup::onBlock(CCObject*){if(m_peer.empty()||m_busy)return;auto peer=m_peer;WeakRef<DirectMessagesPopup> weak=this;createQuickPopup("Bloquear jogador?","Esta pessoa nao podera enviar novas DMs para voce.","Cancelar","Bloquear",[weak,peer](auto,bool yes){auto self=weak.lock();if(!yes||!self)return;self->m_action.rpc("geobr_dm_block",matjson::makeObject({{"p_peer",peer}}),[weak](auto r){if(auto s=weak.lock())s->m_status->setString(r.success?"Jogador bloqueado.":r.error.c_str());});});}
}
