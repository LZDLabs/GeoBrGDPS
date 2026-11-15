#pragma once

#include "../models/MainLevel.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include <cstddef>
#include <vector>

namespace geobr {

class GeoBRMainLevelsLayer final : public cocos2d::CCLayer {
public:
    static cocos2d::CCScene* scene();
    static GeoBRMainLevelsLayer* create();

private:
    cocos2d::CCNode* m_card = nullptr;
    cocos2d::CCLabelBMFont* m_pageLabel = nullptr;
    CCMenuItemSpriteExtra* m_previousButton = nullptr;
    CCMenuItemSpriteExtra* m_nextButton = nullptr;
    CCMenuItemSpriteExtra* m_playButton = nullptr;
    cocos2d::CCNode* m_downloadOverlay = nullptr;
    std::vector<MainLevel> m_levels;
    std::size_t m_index = 0;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_request;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_downloadRequest;
    bool m_launching = false;

    bool init() override;
    void keyBackClicked() override;
    void onBack(cocos2d::CCObject*);
    void onPrevious(cocos2d::CCObject*);
    void onNext(cocos2d::CCObject*);
    void onOpenLevel(cocos2d::CCObject*);
    void onMap(cocos2d::CCObject*);
    void onRefresh(cocos2d::CCObject*);
    void load();
    void pollCatalog(float);
    void showState(std::string const& message, bool loading, bool error = false);
    void render(std::vector<MainLevel> levels);
    void renderCurrent();
    void showDownloadOverlay();
    void hideDownloadOverlay();
};

}
