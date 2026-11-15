#include "ui/GeoBRHubLayer.hpp"
#include "services/AccessService.hpp"
#include "gdps/GdpsService.hpp"
#include "services/GeoPassService.hpp"
#include "services/RankService.hpp"
#include "gameplay/GeoBRRobotPack.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "collab/CollabService.hpp"
using namespace geode::prelude;
// GeoBR works only on the GDPS. On the official server every hook of this mod
// is switched off except the main menu's server button (and update check), so
// GD stays vanilla; the GeoBR admin panel now opens from the GDPS Owner panel.
$on_mod(Loaded){
 if(!geobr::gdps::enabled()){
  for(auto hook:Mod::get()->getHooks())if(hook->getPriority()!=geobr::gdps::ServerMenuHookPriority)(void)hook->disable();
  log::info("GeoBR: servidor oficial, mod em espera (so o botao de servidor fica ativo)");
  return;
 }
 geobr::registerGeoBRRobotPack();
}
class $modify(GeoBRMenuLayer,MenuLayer){
 bool init(){
  if(!MenuLayer::init())return false;geobr::AccessService::get()->start();geobr::GeoPassService::get()->start();geobr::collab::CollabService::get()->start();static bool ranked=false;if(!ranked){ranked=true;geobr::RankService::get()->start();}
  auto sprite=ButtonSprite::create("GeoBR",82,true,"bigFont.fnt","GJ_button_01.png",31,.48f);auto b=CCMenuItemSpriteExtra::create(sprite,this,menu_selector(GeoBRMenuLayer::onOpenGeoBR));b->setID("geobr-button");
  if(auto menu=typeinfo_cast<CCMenu*>(getChildByID("bottom-menu"))){menu->addChild(b);menu->updateLayout();}
  else {auto fallback=CCMenu::create();auto win=CCDirector::sharedDirector()->getWinSize();fallback->setPosition({win.width/2,31});fallback->addChild(b);addChild(fallback,20);}
  return true;
 }
 void onOpenGeoBR(CCObject*){if(!geobr::AccessService::get()->allowed())return;if(auto p=geobr::GeoBRHubLayer::create())p->show();}
};
