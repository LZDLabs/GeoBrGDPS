#include "../ui/GeoBRMainLevelsLayer.hpp"
#include "../ui/GeoPassPopup.hpp"
#include "../ui/TierShopPopup.hpp"
#include "../ui/GeoMapPopup.hpp"
#include "../ui/GeoBRVisuals.hpp"
#include "../ui/UIHelpers.hpp"
#include "../services/AccessService.hpp"
#include <Geode/modify/CreatorLayer.hpp>
using namespace geode::prelude;
class $modify(GeoBRCreatorLayer,CreatorLayer){
 struct Fields{CCNode* page=nullptr;CCMenu* next=nullptr;std::vector<std::pair<Ref<CCNode>,bool>> native;};
 bool init(){
  if(!CreatorLayer::init())return false;auto win=CCDirector::sharedDirector()->getWinSize();
  auto arrow=CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");arrow->setFlipX(true);arrow->setScale(.58f);
  auto button=CCMenuItemSpriteExtra::create(arrow,this,menu_selector(GeoBRCreatorLayer::onGeoBR));button->setID("geobr-section-next");
  auto menu=CCMenu::create();menu->setPosition({win.width-23,win.height/2});menu->addChild(button);menu->setID("geobr-section-navigation");m_fields->next=menu;addChild(menu,30);
  return true;
 }
 void onGeoBR(CCObject*){
  if(!geobr::AccessService::get()->allowed())return;auto& f=*m_fields.operator->();if(f.page)return;auto win=CCDirector::sharedDirector()->getWinSize();
  for(auto child:CCArrayExt<CCNode*>(getChildren())){f.native.emplace_back(child,child->isVisible());child->setVisible(false);}
  auto page=geobr::visual::theme(win,"default");page->setID("geobr-section-page");f.page=page;addChild(page,40);
  auto title=geobr::ui::makeLabel("GeoBR","goldFont.fnt",.9f);title->setPosition({win.width/2,win.height-25});page->addChild(title);
  auto menu=CCMenu::create();menu->setPosition({0,0});page->addChild(menu,5);
  auto back=CCMenuItemSpriteExtra::create(CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png"),this,menu_selector(GeoBRCreatorLayer::onNativePage));back->setPosition({25,win.height-25});menu->addChild(back);
  float tileSize=std::min(108.f,(win.height-80.f)/2.f);float gap=22.f;
  const char* textures[]={"geobr-button-main-levels.png"_spr,"geobr-button-geopass.png"_spr,"geobr-button-tier-shop.png"_spr,"geobr-button-map.png"_spr};
  const char* names[]={"Main Levels","GeoPass","Tier Shop","Mapa"};
  for(int i=0;i<4;++i){auto icon=geobr::visual::tile(textures[i],names[i],tileSize);auto b=CCMenuItemSpriteExtra::create(icon,this,menu_selector(GeoBRCreatorLayer::onSection));b->setTag(i);b->setPosition({win.width/2+(i%2?1:-1)*(tileSize+gap)/2,win.height/2+((i<2)?1:-1)*(tileSize+10)/2-5});menu->addChild(b);}
 }
 void onNativePage(CCObject*){
  auto& f=*m_fields.operator->();if(!f.page)return;f.page->removeFromParent();f.page=nullptr;for(auto const& [node,visible]:f.native)node->setVisible(visible);f.native.clear();
 }
 void keyBackClicked(){if(m_fields->page)onNativePage(nullptr);else CreatorLayer::keyBackClicked();}
 void onSection(CCObject* sender){
  if(!geobr::AccessService::get()->allowed())return;switch(sender->getTag()){
   case 0:CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(.3f,geobr::GeoBRMainLevelsLayer::scene()));break;
   case 1:if(auto p=geobr::GeoPassPopup::create())p->show();break;
   case 2:if(auto p=geobr::TierShopPopup::create())p->show();break;
   case 3:if(auto p=geobr::GeoMapPopup::create())p->show();break;
  }
 }
};
