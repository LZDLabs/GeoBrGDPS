#include "ExtrasPopup.hpp"
#include "UIHelpers.hpp"
#include "GeoBRVisuals.hpp"
#include "ProgressionPopup.hpp"
#include "TierShopPopup.hpp"
#include "../models/JsonModelUtils.hpp"
#include <Geode/ui/ScrollLayer.hpp>
using namespace geode::prelude;
namespace geobr {
ExtrasPopup* ExtrasPopup::create(){auto p=new ExtrasPopup;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
bool ExtrasPopup::init(){if(!Popup::init(470,300))return false;visual::fitPopup(m_mainLayer,{470,300});visual::stylePopup(m_mainLayer,m_bgSprite,{470,300},{37,33,64});setTitle("Opcoes GeoBR");render();return true;}
void ExtrasPopup::render(){if(m_view)m_view->removeFromParent();m_view=CCNode::create();m_mainLayer->addChild(m_view);auto menu=CCMenu::create();menu->setPosition({0,0});m_view->addChild(menu);
 const char* tabs[]={"Interface","Historico","Colecao"};for(int i=0;i<3;++i){auto b=ui::makeTextButton(tabs[i],132,"GJ_button_04.png",this,i==2?menu_selector(ExtrasPopup::onCollection):menu_selector(ExtrasPopup::onTab),.30f,28);b->setPosition({85.f+150*i,246});b->setTag(i);menu->addChild(b);}
 if(!m_tab){auto label=ui::makeLabel("Tamanho da interface","bigFont.fnt",.49f,{255,228,142});label->setPosition({235,197});m_view->addChild(label);for(int i=0;i<2;++i){auto b=ui::makeTextButton(i?"Media":"Compacta",170,Mod::get()->getSettingValue<int64_t>("ui-density")==i?"GJ_button_01.png":"GJ_button_04.png",this,menu_selector(ExtrasPopup::onDensity),.37f,36);b->setPosition({135.f+i*200,143});b->setTag(i);menu->addChild(b);}auto hint=ui::makeLabel("A previa muda agora. Reabra outras telas para aplicar.","chatFont.fnt",.43f,{185,218,240});hint->setPosition({235,91});hint->limitLabelWidth(414,.43f,.28f);m_view->addChild(hint);return;}
 auto list=ScrollLayer::create({424,192},true,true);list->setPosition({23,31});list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(5));m_view->addChild(list);if(!m_rows.isArray())return;auto rows=m_rows.asArray().unwrap();for(int i=0;i<rows.size();++i){auto const& row=rows[i];auto panel=ui::makePanel({412,50},{28,44,68},255);auto stamp=jsonutil::stringOr(row,"opened_at");auto title=ui::makeLabel("Caixa #"+std::to_string(jsonutil::integerOr(row,"rarity"))+" | "+stamp.substr(0,16)+" UTC","bigFont.fnt",.27f,{255,225,140});title->setPosition({206,33});title->limitLabelWidth(392,.27f,.18f);panel->addChild(title);auto hint=ui::makeLabel("Toque para rever os itens recebidos","chatFont.fnt",.42f);hint->setPosition({206,13});panel->addChild(hint);auto button=CCMenuItemSpriteExtra::create(panel,this,menu_selector(ExtrasPopup::onHistory));button->setTag(i);auto m=CCMenu::create();m->setPosition({206,25});m->addChild(button);auto n=CCNode::create();n->setContentSize({412,50});n->setAnchorPoint({.5f,.5f});n->addChild(m);list->m_contentLayer->addChild(n);}list->m_contentLayer->updateLayout();list->scrollToTop();if(rows.empty()){auto empty=ui::makeLabel("Nenhuma caixa aberta ainda.","bigFont.fnt",.4f);empty->setPosition({235,140});m_view->addChild(empty);}}
void ExtrasPopup::onDensity(CCObject* s){Mod::get()->setSettingValue<int64_t>("ui-density",s->getTag());visual::fitPopup(m_mainLayer,{470,300});render();}
void ExtrasPopup::onTab(CCObject* s){m_tab=s->getTag();render();if(m_tab)m_request.call("GET","/rest/v1/geobr_crates?select=id,rarity,opened_at,reward&opened_at=not.is.null&order=opened_at.desc&limit=30",{},[this](auto r){if(r.success)m_rows=r.value;else FLAlertLayer::create("Historico",r.error.c_str(),"OK")->show();render();});}
void ExtrasPopup::onHistory(CCObject* s){if(!m_rows.isArray())return;auto rows=m_rows.asArray().unwrap();int i=s->getTag();if(i<0||i>=rows.size())return;auto r=rows[i].get("reward");if(!r)return;auto items=r.unwrap().get("items");showCrateBundle(items&&items.unwrap().isArray()?items.unwrap():matjson::Value(std::vector<matjson::Value>{r.unwrap()}),jsonutil::integerOr(rows[i],"rarity",1));}
void ExtrasPopup::onCollection(CCObject*){if(auto p=TierShopPopup::create(0,true))p->show();}
}
