#include "ui/GeoBRHubLayer.hpp"
#include "services/AccessService.hpp"
#include "ui/AdminPanel.hpp"
#include "services/GeoPassService.hpp"
#include "services/RankService.hpp"
#include "gameplay/GeoBRRobotPack.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "collab/CollabService.hpp"
using namespace geode::prelude;
$on_mod(Loaded){geobr::registerGeoBRRobotPack();}
class $modify(GeoBRMenuLayer,MenuLayer){
 struct Fields{int taps=0;Ref<CCObject> sender;};
 bool init(){
  if(!MenuLayer::init())return false;geobr::AccessService::get()->start();geobr::GeoPassService::get()->start();geobr::collab::CollabService::get()->start();static bool ranked=false;if(!ranked){ranked=true;geobr::RankService::get()->start();}
  auto sprite=ButtonSprite::create("GeoBR",82,true,"bigFont.fnt","GJ_button_01.png",31,.48f);auto b=CCMenuItemSpriteExtra::create(sprite,this,menu_selector(GeoBRMenuLayer::onOpenGeoBR));b->setID("geobr-button");
  if(auto menu=typeinfo_cast<CCMenu*>(getChildByID("bottom-menu"))){menu->addChild(b);menu->updateLayout();}
  else {auto fallback=CCMenu::create();auto win=CCDirector::sharedDirector()->getWinSize();fallback->setPosition({win.width/2,31});fallback->addChild(b);addChild(fallback,20);}
  return true;
 }
 void onOpenGeoBR(CCObject*){if(!geobr::AccessService::get()->allowed())return;if(auto p=geobr::GeoBRHubLayer::create())p->show();}
 void onMoreGames(CCObject* sender){
  unschedule(schedule_selector(GeoBRMenuLayer::finishMoreGames));m_fields->sender=sender;
  if(++m_fields->taps>=8){m_fields->taps=0;m_fields->sender=nullptr;if(auto p=geobr::AdminLogin::create())p->show();return;}
  scheduleOnce(schedule_selector(GeoBRMenuLayer::finishMoreGames),.8f);
 }
 void finishMoreGames(float){auto sender=m_fields->sender;m_fields->taps=0;m_fields->sender=nullptr;MenuLayer::onMoreGames(sender.data());}
};
