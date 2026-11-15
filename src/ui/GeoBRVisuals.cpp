#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../arrow/ArrowSkins.hpp"
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <algorithm>
#include <array>
#include <random>
#include <string_view>
#define MORE_ICONS_EVENTS
#include <hiimjustin000.more_icons/include/MoreIcons.hpp>
using namespace geode::prelude;
namespace geobr::visual {
void fitPopup(CCNode* layer,CCSize design){auto win=CCDirector::sharedDirector()->getWinSize();float factor=Mod::get()->getSettingValue<int64_t>("ui-density")==0?.78f:.88f;layer->setScale(std::min({1.f,win.width*.90f/design.width,win.height*factor/design.height}));}
bool hasThemeArt(std::string const& s){
 static const std::array<std::string_view,10> Styles{"fire","ice","forest","cosmos","neon","gold","aurora","eclipse","royal","astral"};
 return std::ranges::find(Styles,s)!=Styles.end();
}
void stylePopup(CCNode* mainLayer,CCNode* nativeBackground,CCSize size,ccColor3B color){
 if(nativeBackground)nativeBackground->setVisible(false);
 if(auto old=mainLayer->getChildByID("geobr-popup-frame"))old->removeFromParent();
 // RC6 filled popups with the flat tint colour; dark tints (the Home uses
 // 30,31,54) rendered as an almost black rectangle. The fill is now a lit
 // vertical gradient, like GD's own popup squares.
 auto lift=[](ccColor3B c,float k){auto up=[k](GLubyte v){return static_cast<GLubyte>(std::min(255.f,v+(255.f-v)*k));};return ccColor3B{up(c.r),up(c.g),up(c.b)};};
 auto frame=ui::Frame::create(size,lift(color,.22f),255,true);frame->setID("geobr-popup-frame");frame->setPosition(size/2);mainLayer->addChild(frame,-10);
 auto top=lift(color,.42f);
 auto gradient=CCLayerGradient::create({top.r,top.g,top.b,255},{color.r,color.g,color.b,255});
 gradient->setContentSize({size.width-8,size.height-8});gradient->setPosition({4,4});frame->addChild(gradient,-5);
}
Palette palette(std::string const& s){
 if(s=="aurora")return {{13,94,110},{13,15,51},{99,255,205}};
 if(s=="eclipse")return {{105,23,48},{13,9,26},{255,132,112}};
 if(s=="royal")return {{74,25,120},{16,13,42},{255,215,111}};
 if(s=="astral")return {{26,96,155},{12,23,65},{171,241,255}};
 if(s=="fire")return {{148,28,27},{36,10,28},{255,174,52}};
 if(s=="ice")return {{18,129,183},{12,35,86},{151,247,255}};
 if(s=="forest")return {{21,113,71},{10,33,42},{137,244,115}};
 if(s=="cosmos")return {{81,34,142},{15,14,47},{217,145,255}};
 if(s=="neon")return {{73,12,95},{7,23,51},{28,248,221}};
 if(s=="gold")return {{146,88,15},{44,25,32},{255,225,92}};
 return {{35,78,153},{16,26,67},{105,218,255}};
}
namespace {
// Sutherland-Hodgman: keeps the procedural shapes inside the theme rectangle.
std::vector<CCPoint> clipToRect(std::vector<CCPoint> poly,CCSize size){
 auto pass=[&](auto inside,auto cross){std::vector<CCPoint> out;if(poly.empty())return out;CCPoint prev=poly.back();bool prevIn=inside(prev);
  for(auto const& cur:poly){bool curIn=inside(cur);if(curIn!=prevIn)out.push_back(cross(prev,cur));if(curIn)out.push_back(cur);prev=cur;prevIn=curIn;}return out;};
 auto atX=[](CCPoint a,CCPoint b,float x){float t=(x-a.x)/(b.x-a.x);return CCPoint{x,a.y+(b.y-a.y)*t};};
 auto atY=[](CCPoint a,CCPoint b,float y){float t=(y-a.y)/(b.y-a.y);return CCPoint{a.x+(b.x-a.x)*t,y};};
 poly=pass([](CCPoint p){return p.x>=0;},[&](CCPoint a,CCPoint b){return atX(a,b,0);});
 poly=pass([&](CCPoint p){return p.x<=size.width;},[&](CCPoint a,CCPoint b){return atX(a,b,size.width);});
 poly=pass([](CCPoint p){return p.y>=0;},[&](CCPoint a,CCPoint b){return atY(a,b,0);});
 poly=pass([&](CCPoint p){return p.y<=size.height;},[&](CCPoint a,CCPoint b){return atY(a,b,size.height);});
 return poly;
}
// Liang-Barsky segment clip.
bool clipSegment(CCPoint& a,CCPoint& b,CCSize size){
 float t0=0,t1=1,dx=b.x-a.x,dy=b.y-a.y;float p[]={-dx,dx,-dy,dy},q[]={a.x,size.width-a.x,a.y,size.height-a.y};
 for(int i=0;i<4;++i){if(p[i]==0){if(q[i]<0)return false;continue;}float r=q[i]/p[i];if(p[i]<0)t0=std::max(t0,r);else t1=std::min(t1,r);if(t0>t1)return false;}
 CCPoint start{a.x+dx*t0,a.y+dy*t0},stop{a.x+dx*t1,a.y+dy*t1};a=start;b=stop;return true;
}
}
// RC5-RC7 wrapped every theme in a CCClippingNode with an alpha-threshold
// CCDrawNode stencil. The alpha-test shader replaces the draw node's own
// shader, the stencil writes nothing and everything inside is discarded: the
// GeoBR page in Creator was black and icons placed in themes (Tier Shop
// Arrows, rewards) never showed. Themes are now a plain node that never draws
// outside its rectangle: the artwork is cropped through its texture rect
// (aspect fill) and the procedural shapes are clipped geometrically.
CCNode* theme(CCSize size,std::string const& style){
 auto node=CCNode::create();node->setContentSize(size);node->setAnchorPoint({0,0});auto c=palette(style);
 if(size.width<=0||size.height<=0)return node;
 if(hasThemeArt(style)){
  if(auto sprite=CCSprite::create(Mod::get()->expandSpriteName(fmt::format("geobr-theme-{}.png",style)).data())){
   auto rect=sprite->getTextureRect();
   if(rect.size.width>0&&rect.size.height>0){
    float targetAspect=size.width/size.height;CCRect crop=rect;
    if(rect.size.width/rect.size.height>targetAspect){crop.size.width=rect.size.height*targetAspect;crop.origin.x+=(rect.size.width-crop.size.width)/2;}
    else{crop.size.height=rect.size.width/targetAspect;crop.origin.y+=(rect.size.height-crop.size.height)/2;}
    sprite->setTextureRect(crop,sprite->isTextureRectRotated(),crop.size);
    sprite->setScaleX(size.width/crop.size.width);sprite->setScaleY(size.height/crop.size.height);
    sprite->setPosition(size/2);node->addChild(sprite);
    auto strip=CCLayerColor::create({c.accent.r,c.accent.g,c.accent.b,110},size.width,2);strip->setPosition({0,size.height-2});node->addChild(strip);return node;
   }
  }
 }

 auto bg=CCLayerGradient::create({c.top.r,c.top.g,c.top.b,255},{c.bottom.r,c.bottom.g,c.bottom.b,255});bg->setContentSize(size);bg->setPosition({0,0});node->addChild(bg);

 auto art=CCDrawNode::create();node->addChild(art);
 // CCDrawNode blends premultiplied colours.
 auto pre=[](ccColor4F k){return ccColor4F{k.r*k.a,k.g*k.a,k.b*k.a,k.a};};
 auto draw=[&](std::vector<CCPoint> v,ccColor4F color){v=clipToRect(std::move(v),size);if(v.size()>=3)art->drawPolygon(v.data(),v.size(),pre(color),0,{0,0,0,0});};
 auto segment=[&](CCPoint a,CCPoint b,float r,ccColor4F color){if(clipSegment(a,b,{size.width,size.height}))art->drawSegment(a,b,r,pre(color));};
 auto dot=[&](CCPoint p,float r,ccColor4F color){r=std::min({r,p.x,size.width-p.x,p.y,size.height-p.y});if(r>0)art->drawDot(p,r,pre(color));};
 for(int i=0;i<12;++i){float x=size.width*i/11.f;
  if(style=="cosmos"||style=="default"||style=="royal"){float y=size.height*(.12f+float((i*37)%79)/100.f);float d=i%3==0?3.f:1.5f;draw({{x-d,y},{x,y+d},{x+d,y},{x,y-d}},{.8f,.65f,1.f,.45f});}
  else if(style=="fire"){float h=size.height*(.12f+float((i*13)%20)/100.f);draw({{x-22,0},{x-10,h*.45f},{x-7,h},{x+5,h*.4f},{x+20,0}},{1.f,.35f,.06f,.40f});}
  else if(style=="ice"||style=="astral"){float h=size.height*(.12f+float((i*17)%24)/100.f);draw({{x-17,0},{x-9,h},{x+5,h*.8f},{x+20,0}},{.4f,.85f,1.f,.35f});}
  else if(style=="forest"){float h=size.height*(.2f+float((i*7)%16)/100.f);draw({{x-30,0},{x,h},{x+30,0}},{.05f,.55f,.25f,.5f});}
  else if(style=="neon"){segment({x-30,0},{x+size.height,size.height},1,{.1f,1.f,.9f,.22f});}
  else if(style=="gold"){float y=size.height*(.2f+float((i*11)%60)/100.f);draw({{x-12,y},{x,y+21},{x+12,y},{x,y-21}},{1.f,.8f,.3f,.24f});}
 }

 if(style=="aurora")for(int i=0;i<7;++i){float x=size.width*i/6;draw({{x-30,size.height},{x+25,size.height},{x+70,size.height*.3f},{x+40,size.height*.18f}},{.2f,1.f,.7f,.16f});}
 if(style=="eclipse"){auto center=CCPoint{size.width*.8f,size.height*.72f};dot(center,size.height*.18f,{1.f,.38f,.2f,.8f});dot(center+CCPoint{size.height*.06f,size.height*.02f},size.height*.17f,{.08f,.03f,.12f,1});}
 if(style=="royal"){CCPoint center{size.width*.8f,size.height*.63f};float limit=std::min({center.x,size.width-center.x,center.y,size.height-center.y})-1.f;
  for(int i=0;i<3;++i){float r=std::min(size.height*(.12f+i*.055f),limit);if(r>1)art->drawCircle(center,r,{0,0,0,0},.8f,pre({1.f,.8f,.4f,.25f}),40);}}
 auto strip=CCLayerColor::create({c.accent.r,c.accent.g,c.accent.b,90},size.width,2);strip->setPosition({0,size.height-2});node->addChild(strip);
 return node;
}

CCNode* bar(CCSize size,float ratio,ccColor3B color){
 auto node=CCNode::create();node->setContentSize(size);node->setAnchorPoint({0,0});
 auto bg=CCLayerColor::create({10,13,36,255},size.width,size.height);bg->setAnchorPoint({0,0});node->addChild(bg);
 auto fill=CCLayerColor::create({color.r,color.g,color.b,255},std::max(0.f,(size.width-2)*std::clamp(ratio,0.f,1.f)),size.height-2);fill->setAnchorPoint({0,0});fill->setPosition({1,1});node->addChild(fill);
 return node;
}
CCNode* tile(char const* texture,std::string const& label,float size){
 auto n=CCNode::create();n->setContentSize({size,size});
 if(auto s=CCSprite::create(texture)){s->setPosition({size/2,size/2});s->setScale(size/s->getContentWidth());n->addChild(s);}
 return n; // Approved button textures already contain labels typeset from PUSAB___.otf.
}
CCNode* rewardIcon(matjson::Value const& item,float size){
 CCNode* icon=nullptr;auto kind=jsonutil::stringOr(item,"kind");
 if(kind=="crate")icon=CCSprite::create(Mod::get()->expandSpriteName(fmt::format("geobr-crate-{}.png",std::clamp<int>(jsonutil::integerOr(item,"crate_rarity",1),1,6))).data());
 else if(kind=="theme")icon=theme({size,size},jsonutil::stringOr(item,"style",ui::slice(jsonutil::stringOr(item,"item_id"),6)));
 else if(kind=="arrow"){auto look=arrow::ArrowSettings::localAppearance();look.skin=arrow::skinFromItem(jsonutil::stringOr(item,"item_id"));if(look.skin>0){auto a=arrow::ArrowIcon::create(look);a->setRotation(-45.f);icon=a;}}
 else if(kind=="title")icon=ui::makeLabel("Aa","bigFont.fnt",.5f,{255,223,117});
 else if(kind=="quest_skip")icon=ui::makeLabel("SKIP","bigFont.fnt",.4f,{143,255,189});
 else if(kind=="tokens")icon=CCSprite::create("geobr-token.png"_spr);
 else if(kind=="orbs")icon=CCSprite::createWithSpriteFrameName("currencyOrbIcon_001.png");
 else if(kind=="stars")icon=CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
 else if(kind=="skin"){
  auto typeName=jsonutil::stringOr(item,"icon_type");IconType type=IconType::Cube;
  if(typeName=="ship")type=IconType::Ship;else if(typeName=="ball")type=IconType::Ball;else if(typeName=="ufo")type=IconType::Ufo;
  else if(typeName=="wave")type=IconType::Wave;else if(typeName=="robot")type=IconType::Robot;else if(typeName=="spider")type=IconType::Spider;else if(typeName=="swing")type=IconType::Swing;else if(typeName=="jetpack")type=IconType::Jetpack;
  int id=std::clamp<int>(jsonutil::integerOr(item,"icon_id",1),1,GameManager::sharedState()->countForType(type));
  auto player=SimplePlayer::create(1);player->updatePlayerFrame(id,type);player->setColor({66,224,255});player->setSecondColor({161,85,255});icon=player;
 }
 if(kind=="theme"&&icon)icon->setAnchorPoint({.5f,.5f});
 if(!icon)icon=ui::makeLabel(kind=="orbs"?"Orbs":"XP","bigFont.fnt",.5f,{121,221,255});
 auto bounds=icon->getContentSize();float extent=std::max(bounds.width,bounds.height);if(extent>0)icon->setScale(size/extent);return icon;
}
CCNode* avatar(matjson::Value const& profile,float size){
 auto gm=GameManager::sharedState();int maxID=std::max(1,gm->countForType(IconType::Cube));
 int id=std::clamp<int>(jsonutil::integerOr(profile,"icon_id",1),1,maxID);
 auto player=SimplePlayer::create(1);player->updatePlayerFrame(id,IconType::Cube);
 auto slug=jsonutil::stringOr(profile,"icon_slug");if(!slug.empty())if(auto info=more_icons::getIcon(slug,IconType::Cube))more_icons::updateSimplePlayer(player,info);
 auto decode=[](int64_t value){return ccColor3B{static_cast<GLubyte>((value>>16)&255),static_cast<GLubyte>((value>>8)&255),static_cast<GLubyte>(value&255)};};
 player->setColor(decode(jsonutil::integerOr(profile,"color1",0x42e0ff)));
 player->setSecondColor(decode(jsonutil::integerOr(profile,"color2",0xa155ff)));
 applyGlow(player,profile);
 player->setScale(size/30.f);return player;
}
// RC9: the player's glow, as configured in GD (synced by profile_sync_v3).
void applyGlow(SimplePlayer* player,matjson::Value const& profile){
 if(!player||!jsonutil::boolOr(profile,"glow",false))return;
 auto value=jsonutil::integerOr(profile,"glow_color",0xffffff);
 ccColor3B color{static_cast<GLubyte>((value>>16)&255),static_cast<GLubyte>((value>>8)&255),static_cast<GLubyte>(value&255)};
 player->m_hasGlowOutline=true;player->enableCustomGlowColor(color);player->updateColors();
}
// Any gamemode icon of a profile ("icons": {"ship": 12, ...}), same colours and glow.
CCNode* playerIcon(matjson::Value const& profile,IconType type,float size){
 auto gm=GameManager::sharedState();
 auto key=[&]()->char const*{switch(type){case IconType::Ship:return "ship";case IconType::Ball:return "ball";case IconType::Ufo:return "ufo";case IconType::Wave:return "wave";case IconType::Robot:return "robot";case IconType::Spider:return "spider";case IconType::Swing:return "swing";case IconType::Jetpack:return "jetpack";default:return "";}}();
 int id=1;
 if(type==IconType::Cube)id=jsonutil::integerOr(profile,"icon_id",1);
 else if(auto icons=profile.get("icons");icons&&icons.unwrap().isObject())id=jsonutil::integerOr(icons.unwrap(),key,1);
 id=std::clamp<int>(id,1,std::max(1,gm->countForType(type)));
 auto player=SimplePlayer::create(1);player->updatePlayerFrame(id,type);
 auto decode=[](int64_t value){return ccColor3B{static_cast<GLubyte>((value>>16)&255),static_cast<GLubyte>((value>>8)&255),static_cast<GLubyte>(value&255)};};
 player->setColor(decode(jsonutil::integerOr(profile,"color1",0x42e0ff)));
 player->setSecondColor(decode(jsonutil::integerOr(profile,"color2",0xa155ff)));
 applyGlow(player,profile);
 if(type==IconType::Cube){auto slug=jsonutil::stringOr(profile,"icon_slug");if(!slug.empty())if(auto info=more_icons::getIcon(slug,IconType::Cube))more_icons::updateSimplePlayer(player,info);}
 player->setScale(size/30.f);return player;
}
std::string uuid(){
 std::array<unsigned char,16> b;std::random_device random;for(auto& x:b)x=static_cast<unsigned char>(random());b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;
 std::string s;for(int i=0;i<16;++i){if(i==4||i==6||i==8||i==10)s+='-';s+=fmt::format("{:02x}",b[i]);}return s;
}
}
