#pragma once
// Picker for one of the player's own levels (Criar > Meus levels), 6 per page.
// Used by the admin panel (Main Levels / Natal) and the Natal owner menu.
#include "UIHelpers.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LocalLevelManager.hpp>
#include <Geode/ui/Popup.hpp>

#include <functional>
#include <vector>

namespace geobr {
using namespace geode::prelude;

// One of my local levels, 6 per page.
class LocalLevelChooser final:public geode::Popup{
    std::function<void(GJGameLevel*)> m_done;std::vector<Ref<GJGameLevel>> m_levels;int m_page=0;CCMenu* m_list=nullptr;
public:
    static LocalLevelChooser* create(std::function<void(GJGameLevel*)> done){auto p=new LocalLevelChooser;if(p->init(std::move(done))){p->autorelease();return p;}delete p;return nullptr;}
    bool init(std::function<void(GJGameLevel*)> done){
        if(!Popup::init(340,260))return false;m_done=std::move(done);setTitle("Escolha um level seu");
        if(auto levels=LocalLevelManager::sharedState()->m_localLevels)for(auto level:CCArrayExt<GJGameLevel*>(levels))if(level&&!std::string(level->m_levelString).empty())m_levels.emplace_back(level);
        auto nav=CCMenu::create();nav->setPosition({0,0});m_mainLayer->addChild(nav,3);
        auto prev=CCMenuItemSpriteExtra::create(CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png"),this,menu_selector(LocalLevelChooser::onPrev));prev->setScale(.6f);prev->setPosition({22,130});nav->addChild(prev);
        auto nextSprite=CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");nextSprite->setFlipX(true);
        auto next=CCMenuItemSpriteExtra::create(nextSprite,this,menu_selector(LocalLevelChooser::onNext));next->setScale(.6f);next->setPosition({318,130});nav->addChild(next);
        render();return true;
    }
    void render(){
        if(m_list)m_list->removeFromParent();m_list=CCMenu::create();m_list->setPosition({0,0});m_mainLayer->addChild(m_list,2);
        int pages=std::max<int>(1,(static_cast<int>(m_levels.size())+5)/6);m_page=std::clamp(m_page,0,pages-1);
        if(m_levels.empty()){auto l=CCLabelBMFont::create("Voce nao tem levels criados.","chatFont.fnt");l->setScale(.6f);l->setPosition({170,130});m_list->addChild(l);return;}
        for(int i=0;i<6;++i){size_t index=m_page*6+i;if(index>=m_levels.size())break;
            auto name=std::string(m_levels[index]->m_levelName);if(name.empty())name="Sem nome";
            auto b=ui::makeTextButton(name.substr(0,24),230,"GJ_button_04.png",this,menu_selector(LocalLevelChooser::onPick),.32f,26);b->setTag(static_cast<int>(index));b->setPosition({170,200-i*30.f});m_list->addChild(b);}
        auto page=CCLabelBMFont::create(fmt::format("{}/{}",m_page+1,pages).c_str(),"goldFont.fnt");page->setScale(.4f);page->setPosition({170,18});m_list->addChild(page);
    }
    void onPrev(CCObject*){--m_page;render();}
    void onNext(CCObject*){++m_page;render();}
    void onPick(CCObject* s){auto index=static_cast<size_t>(s->getTag());if(index>=m_levels.size())return;Ref<GJGameLevel> level=m_levels[index];auto done=m_done;onClose(nullptr);if(done)done(level);}
};

} // namespace geobr
