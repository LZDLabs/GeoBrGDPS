#include "GeoBRHubLayer.hpp"
#include "NewsTab.hpp"
#include "CommunityTab.hpp"
#include "UIHelpers.hpp"
#include "GeoBRVisuals.hpp"
#include "DirectMessagesPopup.hpp"
#include "GeoPlayersPopup.hpp"
#include "CollabPopup.hpp"
#include "GdpsPopups.hpp"
#include "GeoPassPopup.hpp"
#include "ProgressionPopup.hpp"
#include "ExtrasPopup.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/AccessService.hpp"
#include "../services/GeoPassService.hpp"
using namespace geode::prelude;
namespace geobr {
namespace {
constexpr CCSize HubSize {530, 330};
struct Shortcut { char const* label; char const* bg; };
constexpr Shortcut Shortcuts[] = {
    {"Passe", "GJ_button_05.png"}, {"Levels", "GJ_button_04.png"}, {"Baus", "GJ_button_04.png"},
    {"DMs", "GJ_button_03.png"}, {"Jogadores", "GJ_button_04.png"}, {"Collab", "GJ_button_02.png"}, {"GDPS", "GJ_button_06.png"}, {"Opcoes", "GJ_button_04.png"},
};
}

GeoBRHubLayer* GeoBRHubLayer::create(){auto p=new GeoBRHubLayer;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}

bool GeoBRHubLayer::init(){
    if(!Popup::init(HubSize.width,HubSize.height))return false;
    visual::fitPopup(m_mainLayer,HubSize);
    visual::stylePopup(m_mainLayer,m_bgSprite,HubSize,{48,40,92});
    // Same approved backdrop as the other GeoBR screens.
    auto backdrop=visual::theme({HubSize.width-14,HubSize.height-14},"cosmos");backdrop->setPosition({7,7});m_mainLayer->addChild(backdrop,-1);
    setID("geobr-home");
    setTitle("GeoBR","goldFont.fnt",.75f,15);

    // Player strip: who is playing, level and Tokens at a glance.
    auto const& state=GeoPassService::get()->state();
    auto profile=state.get("profile");
    auto p=profile?profile.unwrap():matjson::Value::object();
    auto avatar=visual::avatar(p,20);avatar->setPosition({34,312});m_mainLayer->addChild(avatar,3);
    auto name=ui::makeLabel(jsonutil::stringOr(p,"display_name","Jogador GeoBR"),"bigFont.fnt",.3f,ui::palette::TextGold);
    name->setAnchorPoint({0,.5f});name->setPosition({50,316});name->limitLabelWidth(150,.3f,.18f);m_mainLayer->addChild(name,3);
    auto level=ui::makeLabel(fmt::format("Nv. {}",jsonutil::integerOr(p,"level",1)),"bigFont.fnt",.22f,ui::palette::TextSoft);
    level->setAnchorPoint({0,.5f});level->setPosition({50,304});m_mainLayer->addChild(level,3);
    if(auto token=CCSprite::create("geobr-token.png"_spr)){token->setScale(18.f/token->getContentWidth());token->setPosition({448,311});m_mainLayer->addChild(token,3);}
    auto tokens=ui::makeLabel(ui::compactNumber(jsonutil::integerOr(state,"tokens")),"bigFont.fnt",.32f,ui::palette::TextGold);
    tokens->setAnchorPoint({0,.5f});tokens->setPosition({462,311});m_mainLayer->addChild(tokens,3);

    auto menu=CCMenu::create();menu->setPosition({0,0});m_mainLayer->addChild(menu,5);
    const char* labels[]={"Noticias e avisos","Chat geral"};
    for(int i=0;i<2;++i){
        auto b=ui::makeTextButton(labels[i],238,i?"GJ_button_04.png":"GJ_button_01.png",this,menu_selector(GeoBRHubLayer::onTab),.36f,28);
        b->setTag(i);b->setPosition({143.f+244*i,277});menu->addChild(b);m_buttons[i]=b;
    }
    m_tabs={NewsTab::create({488,203}),CommunityTab::create({488,203})};
    for(int i=0;i<2;++i){if(!m_tabs[i])return false;m_tabs[i]->setPosition({265,155});m_tabs[i]->setVisible(i==0);m_mainLayer->addChild(m_tabs[i],2);}

    constexpr int count=sizeof(Shortcuts)/sizeof(Shortcuts[0]);
    for(int i=0;i<count;++i){
        auto b=ui::makeTextButton(Shortcuts[i].label,56,Shortcuts[i].bg,this,menu_selector(GeoBRHubLayer::onShortcut),.23f,30);
        b->setPosition({265.f+(i-(count-1)/2.f)*61.f,27});b->setTag(i);menu->addChild(b);
    }
    selectTab(0);
    return true;
}

void GeoBRHubLayer::selectTab(int tab){
    for(int i=0;i<2;++i){
        m_tabs[i]->setVisible(i==tab);
        m_buttons[i]->setOpacity(i==tab?255:150);
        m_buttons[i]->setScale(i==tab?1.f:.96f);
    }
}

void GeoBRHubLayer::onTab(CCObject* s){selectTab(s->getTag());}

void GeoBRHubLayer::onShortcut(CCObject* s){
    if(!AccessService::get()->allowed())return;
    switch(s->getTag()){
        case 0:if(auto p=GeoPassPopup::create())p->show();break;
        case 1:if(auto p=ProgressionPopup::create())p->show();break;
        case 2:if(auto p=ProgressionPopup::create(1))p->show();break;
        case 3:if(auto p=DirectMessagesPopup::create())p->show();break;
        case 4:if(auto p=GeoPlayersPopup::create())p->show();break;
        case 5:if(auto p=CollabPopup::create())p->show();break;
        case 6:if(auto p=GdpsPopup::create())p->show();break;
        case 7:if(auto p=ExtrasPopup::create())p->show();break;
    }
}
}
