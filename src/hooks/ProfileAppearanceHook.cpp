#include "../services/GeoPassService.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../ui/GeoBRVisuals.hpp"
#include "../ui/UIHelpers.hpp"
#include <Geode/modify/ProfilePage.hpp>
#include <cmath>
using namespace geode::prelude;
namespace {
void tintSurfaces(CCNode* root,ccColor3B color,int depth=0){
 if(!root||depth>12||root->getID()=="geobr-profile-background")return;
 auto size=root->getScaledContentSize();
 if(size.width>120&&size.height>16){
  if(auto patch=typeinfo_cast<extension::CCScale9Sprite*>(root))patch->setColor(color);
  else if(auto layer=typeinfo_cast<CCLayerColor*>(root))layer->setColor(color);
 }
 for(auto c:CCArrayExt<CCNode*>(root->getChildren()))tintSurfaces(c,color,depth+1);
}
}

class $modify(GeoBRProfileAppearance,ProfilePage){
 struct Fields{geobr::CloudRequest request;Ref<CCNode> backdrop;std::string style="default";};
 void applyCosmetic(matjson::Value const& profile){
  if(!m_usernameLabel||!m_usernameLabel->getParent())return;auto parent=m_usernameLabel->getParent();
  if(auto old=parent->getChildByID("geobr-native-cosmetic"))old->removeFromParent();
  if(m_fields->backdrop){m_fields->backdrop->removeFromParent();m_fields->backdrop=nullptr;}
  auto style=geobr::jsonutil::stringOr(profile,"theme_slug","default");auto colors=geobr::visual::palette(style);m_fields->style=style;
  if(style!="default"){
   CCNode* panel=nullptr;float largest=0;
   for(auto child:CCArrayExt<CCNode*>(m_mainLayer->getChildren())){
    if(typeinfo_cast<CCMenu*>(child)||typeinfo_cast<CCLayer*>(child))continue;
    auto size=child->getScaledContentSize();float area=size.width*size.height;
    if(size.width>=300&&size.height>=200&&area>largest){panel=child;largest=area;}
   }
   if(panel){
    // Local coordinates follow the native frame, including scale and relocation.
    // Clipped inset artwork cannot leak over the frame or drift when comments scroll.
    auto size=panel->getContentSize();float inset=6/std::max(.1f,panel->getScaleX());
    auto bg=geobr::visual::theme({size.width-inset*2,size.height-inset*2},style);bg->setPosition({inset,inset});bg->setID("geobr-profile-background");panel->addChild(bg,1);m_fields->backdrop=bg;
   }
   for(auto child:CCArrayExt<CCNode*>(m_mainLayer->getChildren())){
    if(child==panel)continue;
    if(auto p=typeinfo_cast<extension::CCScale9Sprite*>(child);p&&p->getScaledContentWidth()>150&&p->getScaledContentHeight()>25)p->setColor(colors.bottom);
   }
   if(m_list)tintSurfaces(m_list,colors.bottom);
   m_usernameLabel->setColor(colors.accent);
  }
  auto node=CCNode::create();node->setID("geobr-native-cosmetic");node->setPosition(m_usernameLabel->getPosition()+CCPoint{0,-18});parent->addChild(node,15);
  auto title=geobr::jsonutil::stringOr(profile,"title_name");auto xp=std::max<int64_t>(0,geobr::jsonutil::integerOr(profile,"lifetime_xp"));title+=fmt::format(" | Nv. {}",int(std::sqrt(double(xp)/100))+1);
  auto label=geobr::ui::makeLabel(title,"bigFont.fnt",.24f,colors.accent);label->limitLabelWidth(230,.24f,.17f);node->addChild(label);
 }
 void setupCommentsBrowser(CCArray* comments){ProfilePage::setupCommentsBrowser(comments);if(m_fields->style!="default"&&m_list)tintSurfaces(m_list,geobr::visual::palette(m_fields->style).bottom);}
 void loadPageFromUserInfo(GJUserScore* score){ProfilePage::loadPageFromUserInfo(score);if(!score)return;if(m_ownProfile){auto profile=geobr::GeoPassService::get()->state().get("profile");if(profile)applyCosmetic(profile.unwrap());}int account=score->m_accountID;if(account<=0)return;m_fields->request.rpc("geobr_native_profile",matjson::makeObject({{"p_account",account}}),[this](auto r){if(r.success&&r.value.isObject()&&r.value.contains("display_name"))applyCosmetic(r.value);});}
};
