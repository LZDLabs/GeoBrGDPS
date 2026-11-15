#include "ProfilePreviewPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../services/GeoPassService.hpp"
#include "../models/JsonModelUtils.hpp"
using namespace geode::prelude;
namespace geobr {
ProfilePreviewPopup* ProfilePreviewPopup::create(std::string const& style){auto p=new ProfilePreviewPopup;if(p->init(style)){p->autorelease();return p;}delete p;return nullptr;}
bool ProfilePreviewPopup::init(std::string const& style){
 if(!Popup::init(440,280))return false;visual::fitPopup(m_mainLayer,{440,280});visual::stylePopup(m_mainLayer,m_bgSprite,{440,280},visual::palette(style).bottom);auto bg=visual::theme({426,266},style);bg->setPosition({7,7});m_mainLayer->addChild(bg,-1);m_bgSprite->setZOrder(-3);
 auto profile=GeoPassService::get()->state().get("profile");auto p=profile?profile.unwrap():matjson::Value::object();auto colors=visual::palette(style);
 auto name=ui::makeLabel(jsonutil::stringOr(p,"display_name","GeoBR"),"bigFont.fnt",.65f,colors.accent);name->setPosition({220,244});name->limitLabelWidth(330,.65f,.3f);m_mainLayer->addChild(name);
 auto title=ui::makeLabel(jsonutil::stringOr(p,"title_name","Seu titulo")+fmt::format(" | Nv. {}",jsonutil::integerOr(p,"level",1)),"bigFont.fnt",.28f);title->setPosition({220,218});title->limitLabelWidth(330,.28f,.18f);m_mainLayer->addChild(title);
 auto panel=ui::makePanel({340,72},colors.bottom,230);panel->setPosition({220,161});m_mainLayer->addChild(panel);auto icon=visual::avatar(p,44);icon->setPosition({70,36});panel->addChild(icon);auto hint=ui::makeLabel("Seu perfil GeoBR","bigFont.fnt",.38f);hint->setPosition({223,36});panel->addChild(hint);
 auto comment=ui::makePanel({340,59},colors.bottom,230);comment->setPosition({220,86});m_mainLayer->addChild(comment);auto body=ui::makeLabel("Previa do fundo e dos paineis","chatFont.fnt",.54f);body->setPosition({170,30});comment->addChild(body);
 auto info=ui::makeLabel("Previa visual - nenhum item foi equipado","chatFont.fnt",.38f,{205,225,239});info->setPosition({220,28});m_mainLayer->addChild(info);return true;
}
}
