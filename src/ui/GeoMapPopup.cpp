#include "GeoMapPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
using namespace geode::prelude;
namespace geobr {
GeoMapPopup* GeoMapPopup::create(){auto p=new GeoMapPopup;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
bool GeoMapPopup::init(){
 if(!Popup::init(360,220))return false;visual::fitPopup(m_mainLayer,{360,220});visual::stylePopup(m_mainLayer,m_bgSprite,{360,220},{53,39,78});setTitle("Mapa GeoBR","goldFont.fnt",.8f,18);
 auto bg=visual::theme({346,206},"cosmos");bg->setPosition({7,7});m_mainLayer->addChild(bg,-1);
 auto icon=CCSprite::create("geobr-button-map.png"_spr);icon->setScale(72.f/icon->getContentWidth());icon->setPosition({180,125});m_mainLayer->addChild(icon);
 auto text=ui::makeLabel("EM BREVE","bigFont.fnt",.65f,{255,229,127});text->setPosition({180,65});m_mainLayer->addChild(text);
 auto sub=ui::makeLabel("Uma nova jornada esta sendo preparada.","chatFont.fnt",.55f);sub->setPosition({180,35});sub->limitLabelWidth(310,.55f,.3f);m_mainLayer->addChild(sub);return true;
}}
