#include "GeoBRMainLevelsLayer.hpp"
#include "../services/AccessService.hpp"

#include "UIHelpers.hpp"
#include "GeoMapPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../services/ApiClient.hpp"
#include "../services/MainLevelLauncher.hpp"
#include "../services/MainLevelProgress.hpp"
#include "../services/MainLevelsService.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/GJDifficultySprite.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/ui/LoadingSpinner.hpp>
#include <Geode/ui/TextArea.hpp>

#include <algorithm>
#include <fmt/format.h>

using namespace geode::prelude;

namespace geobr {

CCScene* GeoBRMainLevelsLayer::scene() {
    auto scene = CCScene::create();
    scene->addChild(GeoBRMainLevelsLayer::create());
    return scene;
}

GeoBRMainLevelsLayer* GeoBRMainLevelsLayer::create() {
    auto result = new GeoBRMainLevelsLayer;
    if (result->init()) {
        result->autorelease();
        return result;
    }
    delete result;
    return nullptr;
}

bool GeoBRMainLevelsLayer::init() {
    if (!CCLayer::init()) return false;

    auto win = CCDirector::sharedDirector()->getWinSize();
    setContentSize(win);
    setKeypadEnabled(true);
    setID("geobr-main-levels-layer");

    auto color = CCLayerColor::create({10, 35, 78, 255});
    color->setContentSize(win);
    addChild(color, -5);

    if (auto background = CCSprite::create("game_bg_05_001.png")) {
        auto textureSize = background->getContentSize();
        if (textureSize.width > 0.f && textureSize.height > 0.f) {
            background->setScaleX(win.width / textureSize.width);
            background->setScaleY(win.height / textureSize.height);
        }
        background->setPosition(win / 2.f);
        background->setColor({75, 130, 210});
        addChild(background, -4);
    }

    auto title = ui::makeLabel("GeoBR Main Levels", "goldFont.fnt", .82f);
    title->setPosition({win.width / 2.f, win.height - 24.f});
    addChild(title, 5);

    auto subtitle = ui::makeLabel("Campanha oficial online", "chatFont.fnt", .56f, {165, 220, 255});
    subtitle->setPosition({win.width / 2.f, win.height - 45.f});
    addChild(subtitle, 5);

    auto topMenu = CCMenu::create();
    topMenu->setPosition({0.f, 0.f});
    topMenu->setContentSize(win);
    topMenu->setID("geobr-main-levels-top-menu");
    auto back = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png"),
        this,
        menu_selector(GeoBRMainLevelsLayer::onBack)
    );
    back->setPosition({25.f, win.height - 25.f});
    back->setID("geobr-main-levels-back-button");
    topMenu->addChild(back);
    auto refresh = ui::makeTextButton(
        "Atualizar", 86, "GJ_button_01.png", this,
        menu_selector(GeoBRMainLevelsLayer::onRefresh), .38f, 28.f
    );
    refresh->setPosition({win.width - 52.f, win.height - 25.f});
    refresh->setID("geobr-main-levels-refresh-button");
    topMenu->addChild(refresh);
    auto map=ui::makeTextButton("Mapa",66,"GJ_button_04.png",this,menu_selector(GeoBRMainLevelsLayer::onMap),.32f,27);map->setPosition({win.width-140,win.height-25});topMenu->addChild(map);
    addChild(topMenu, 10);

    CCSize cardSize {
        std::min(438.f, win.width - 112.f),
        std::min(218.f, win.height - 88.f)
    };
    m_card = CCNode::create();
    m_card->setContentSize(cardSize);
    m_card->setAnchorPoint({.5f, .5f});
    m_card->setPosition({win.width / 2.f, win.height / 2.f - 3.f});
    m_card->setID("geobr-main-level-card");
    addChild(m_card, 3);

    auto navigation = CCMenu::create();
    navigation->setPosition({0.f, 0.f});
    navigation->setContentSize(win);
    navigation->setID("geobr-main-level-navigation");
    m_previousButton = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png"),
        this,
        menu_selector(GeoBRMainLevelsLayer::onPrevious)
    );
    m_previousButton->setPosition({35.f, win.height / 2.f - 3.f});
    m_previousButton->setID("geobr-main-level-previous");
    navigation->addChild(m_previousButton);
    auto nextSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    nextSprite->setFlipX(true);
    m_nextButton = CCMenuItemSpriteExtra::create(
        nextSprite, this, menu_selector(GeoBRMainLevelsLayer::onNext)
    );
    m_nextButton->setPosition({win.width - 35.f, win.height / 2.f - 3.f});
    m_nextButton->setID("geobr-main-level-next");
    navigation->addChild(m_nextButton);
    addChild(navigation, 8);

    m_pageLabel = ui::makeLabel("", "bigFont.fnt", .42f, {205, 230, 255});
    m_pageLabel->setPosition({win.width / 2.f, 17.f});
    addChild(m_pageLabel, 5);

    load();
    schedule(schedule_selector(GeoBRMainLevelsLayer::pollCatalog),20.f);
    return true;
}

void GeoBRMainLevelsLayer::keyBackClicked() {
    onBack(nullptr);
}

void GeoBRMainLevelsLayer::onBack(CCObject*) {
    m_request.cancel();
    m_downloadRequest.cancel();
    CCDirector::sharedDirector()->popSceneWithTransition(.35f, PopTransition::kPopTransitionFade);
}

void GeoBRMainLevelsLayer::onPrevious(CCObject*) {
    if (m_index == 0 || m_levels.empty()) return;
    --m_index;
    renderCurrent();
}

void GeoBRMainLevelsLayer::onNext(CCObject*) {
    if (m_levels.empty() || m_index + 1 >= m_levels.size()) return;
    ++m_index;
    renderCurrent();
}

void GeoBRMainLevelsLayer::onOpenLevel(CCObject*) {
    if (m_levels.empty() || m_launching || !AccessService::get()->allowed()) return;
    auto level = m_levels[m_index];
    if (!GeoBRConfig::validStoragePath(level.gmdPath)) {
        FLAlertLayer::create(
            "Arquivo invalido",
            "O caminho do <cy>.gmd</c> deste level esta invalido no Supabase.",
            "OK"
        )->show();
        return;
    }

    if(auto local=MainLevelLauncher::cached(level);local){CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(.3f,PlayLayer::scene(local.level,false,false)));return;}
    m_launching = true;
    showDownloadOverlay();
    m_downloadRequest.spawn("GeoBR - baixar main level", MainLevelLauncher::download(level), [this, level](auto response) {
        if(!AccessService::get()->allowed()){m_launching=false;hideDownloadOverlay();return;}
        auto imported = MainLevelLauncher::import(level, response);
        m_launching = false;
        hideDownloadOverlay();
        if (!imported) {
            FLAlertLayer::create("Falha ao abrir level", imported.error, "OK")->show();
            return;
        }
        CCDirector::sharedDirector()->replaceScene(
            CCTransitionFade::create(.45f, PlayLayer::scene(imported.level, false, false))
        );
    });
}

void GeoBRMainLevelsLayer::onMap(CCObject*){if(!m_launching)if(auto p=GeoMapPopup::create())p->show();}

void GeoBRMainLevelsLayer::onRefresh(CCObject*) {
    if (m_launching) return;
    load();
}

void GeoBRMainLevelsLayer::pollCatalog(float){
    if(m_launching||m_request.isPending())return;
    m_request.spawn("GeoBR - atualizar campanha",MainLevelsService::fetch(),[this](auto response){
        if(m_launching)return;
        // Only rebuild the card when the published catalog really changed.
        auto before=Mod::get()->getSavedValue<matjson::Value>("campaign-catalog",matjson::Value::array()).dump();
        auto result=MainLevelsService::parse(response);if(!result.success)return;
        if(!m_levels.empty()&&Mod::get()->getSavedValue<matjson::Value>("campaign-catalog",matjson::Value::array()).dump()==before)return;
        auto current=m_levels.empty()?0:m_levels[m_index].id;
        render(std::move(result.data));
        for(size_t i=0;i<m_levels.size();++i)if(m_levels[i].id==current){m_index=i;renderCurrent();break;}
    });
}
void GeoBRMainLevelsLayer::load() {
    m_request.cancel();
    m_downloadRequest.cancel();
    m_launching = false;
    hideDownloadOverlay();
    if (auto error = GeoBRConfig::supabaseConfigurationError(); !error.empty()) {
        showState(error, false, true);
        return;
    }

    showState("Carregando main levels...", true);
    m_request.spawn("GeoBR - main levels", MainLevelsService::fetch(), [this](auto response) {
        auto result = MainLevelsService::parse(response);
        if (!result.success){auto cache=MainLevelsService::cached();if(!cache.empty())render(std::move(cache));else showState(result.error,false,true);}
        else render(std::move(result.data));
    });
}

void GeoBRMainLevelsLayer::showState(std::string const& message, bool loading, bool error) {
    m_levels.clear();
    m_index = 0;
    m_playButton = nullptr;
    m_card->removeAllChildrenWithCleanup(true);

    auto panel = ui::makePanel(m_card->getContentSize(), {18, 48, 91}, 235);
    panel->setPosition(m_card->getContentSize() / 2.f);
    m_card->addChild(panel);

    auto label = ui::makeLabel(
        message, "bigFont.fnt", .43f,
        error ? ccColor3B {255, 150, 135} : ccColor3B {255, 255, 255}
    );
    label->limitLabelWidth(m_card->getContentWidth() - 55.f, .43f, .25f);
    label->setPosition({m_card->getContentWidth() / 2.f, m_card->getContentHeight() / 2.f - (loading ? 23.f : 0.f)});
    m_card->addChild(label, 2);
    if (loading) {
        auto spinner = LoadingSpinner::create(38.f);
        spinner->setPosition({m_card->getContentWidth() / 2.f, m_card->getContentHeight() / 2.f + 22.f});
        m_card->addChild(spinner, 2);
    }

    m_previousButton->setEnabled(false);
    m_nextButton->setEnabled(false);
    m_pageLabel->setString("");
}

void GeoBRMainLevelsLayer::render(std::vector<MainLevel> levels) {
    if (levels.empty()) {
        showState("Nenhum main level disponivel.", false);
        return;
    }
    m_levels = std::move(levels);
    m_index = 0;
    renderCurrent();
}

void GeoBRMainLevelsLayer::renderCurrent() {
    if (m_levels.empty()) return;
    auto const& level = m_levels[m_index];
    auto size = m_card->getContentSize();
    m_card->removeAllChildrenWithCleanup(true);

    m_playButton = nullptr;
    auto panel = ui::makePanel(size, {29, 50, 104}, 248);
    panel->setPosition(size / 2.f);
    m_card->addChild(panel);

    auto leftWidth = 102.f;
    auto difficultyPanel = ui::makePanel({leftWidth - 12.f, size.height - 28.f}, {9, 31, 68}, 235);
    difficultyPanel->setPosition({leftWidth / 2.f + 2.f, size.height / 2.f - 1.f});
    m_card->addChild(difficultyPanel, 1);

    auto chapter = ui::makeLabel(
        fmt::format("LEVEL {}", level.order > 0 ? level.order : static_cast<int>(m_index + 1)),
        "bigFont.fnt", .34f, {130, 215, 255}
    );
    chapter->setPosition({leftWidth + (size.width - leftWidth) / 2.f, size.height - 17.f});
    m_card->addChild(chapter, 2);

    auto name = ui::makeLabel(level.name, "goldFont.fnt", .68f);
    name->limitLabelWidth(size.width - leftWidth - 30.f, .68f, .38f);
    name->setPosition({leftWidth + (size.width - leftWidth) / 2.f, size.height - 42.f});
    m_card->addChild(name, 2);

    auto creator = ui::makeLabel(fmt::format("por {}", level.creator), "bigFont.fnt", .32f, {190, 220, 255});
    creator->limitLabelWidth(size.width - leftWidth - 36.f, .32f, .21f);
    creator->setPosition({leftWidth + (size.width - leftWidth) / 2.f, size.height - 66.f});
    m_card->addChild(creator, 2);

    auto difficultyCode = level.difficultyCode();
    if (auto face = GJDifficultySprite::create(difficultyCode, GJDifficultyName::Short)) {
        face->setScale(.86f);
        face->setPosition({leftWidth / 2.f + 2.f, size.height - 60.f});
        m_card->addChild(face, 3);
    }
    auto difficulty = ui::makeLabel(level.difficulty, "bigFont.fnt", .34f, {255, 255, 255});
    difficulty->limitLabelWidth(leftWidth - 24.f, .34f, .23f);
    difficulty->setPosition({leftWidth / 2.f + 2.f, size.height - 99.f});
    m_card->addChild(difficulty, 3);

    auto modeBadge = ui::makePanel({76.f, 21.f}, level.platformer ? ccColor3B{75, 135, 80} : ccColor3B{38, 112, 170}, 245);
    modeBadge->setPosition({leftWidth / 2.f + 2.f, 47.f});
    m_card->addChild(modeBadge, 2);
    auto mode = ui::makeLabel(level.modeLabel(), "bigFont.fnt", .25f, {230, 255, 235});
    mode->setPosition({leftWidth / 2.f + 2.f, 47.f});
    m_card->addChild(mode, 3);

    if (auto star = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png")) {
        star->setScale(.62f);
        star->setPosition({leftWidth / 2.f - 8.f, 22.f});
        m_card->addChild(star, 3);
        auto stars = ui::makeLabel(fmt::to_string(level.stars), "bigFont.fnt", .43f, {255, 235, 105});
        stars->setAnchorPoint({0.f, .5f});
        stars->setPosition({leftWidth / 2.f + 5.f, 22.f});
        m_card->addChild(stars, 3);
    }

    auto description = SimpleTextArea::create(
        level.description, "chatFont.fnt", .55f, size.width - leftWidth - 38.f
    );
    description->setAnchorPoint({.5f, 1.f});
    description->setAlignment(kCCTextAlignmentCenter);
    description->setWrappingMode(WrappingMode::WORD_WRAP);
    description->setMaxLines(3);
    description->setPosition({leftWidth + (size.width - leftWidth) / 2.f, size.height - 82.f});
    m_card->addChild(description, 2);

    auto progress = MainLevelProgress::load(level.id);
    auto progressLabel = ui::makeLabel(
        fmt::format("NORMAL  {}%    PRATICA  {}%", progress.normal, progress.practice),
        "bigFont.fnt", .29f, {150, 235, 255}
    );
    progressLabel->setAnchorPoint({0.f, .5f});
    progressLabel->setPosition({leftWidth + 12.f, 72.f});
    progressLabel->limitLabelWidth(size.width - leftWidth - 24.f, .29f, .21f);
    m_card->addChild(progressLabel, 3);
    auto progressBar=visual::bar({size.width-leftWidth-24,5},progress.normal/100.f,{105,228,90});progressBar->setPosition({leftWidth+12,62});m_card->addChild(progressBar,3);

    for (int coin = 0; coin < level.coins; ++coin) {
        if (auto icon = CCSprite::createWithSpriteFrameName("GJ_coinsIcon_001.png")) {
            icon->setScale(.42f);
            icon->setPosition({leftWidth + 18.f + coin * 22.f, 52.f});
            m_card->addChild(icon, 3);
        }
    }
    auto coinLabel = ui::makeLabel(
        level.coins > 0 ? "SECRET COINS" : "SEM MOEDAS",
        "chatFont.fnt", .39f, level.coins > 0 ? ccColor3B{255, 224, 90} : ccColor3B{155, 185, 210}
    );
    coinLabel->setAnchorPoint({0.f, .5f});
    coinLabel->setPosition({leftWidth + 18.f + level.coins * 22.f, 52.f});
    m_card->addChild(coinLabel, 3);

    auto song = ui::makeLabel(
        fmt::format("Musica: {}", level.song), "chatFont.fnt", .49f, {165, 210, 245}
    );
    song->limitLabelWidth(size.width - leftWidth - 145.f, .49f, .28f);
    song->setAnchorPoint({0.f, .5f});
    song->setPosition({leftWidth + 12.f, 31.f});
    m_card->addChild(song, 2);

    auto id = ui::makeLabel(fmt::format("GeoBR #{}  |  arquivo v{}", level.id, level.fileVersion), "chatFont.fnt", .42f, {130, 178, 220});
    id->setAnchorPoint({0.f, .5f});
    id->setPosition({leftWidth + 12.f, 14.f});
    m_card->addChild(id, 2);

    auto playMenu = CCMenu::create();
    playMenu->setPosition({size.width - 55.f, 22.f});
    m_playButton = ui::makeTextButton(
        "JOGAR", 92, "GJ_button_01.png", this,
        menu_selector(GeoBRMainLevelsLayer::onOpenLevel), .38f, 30.f
    );
    m_playButton->setID("geobr-main-level-play-button");
    playMenu->addChild(m_playButton);
    m_card->addChild(playMenu, 4);

    m_previousButton->setEnabled(m_index > 0);
    m_nextButton->setEnabled(m_index + 1 < m_levels.size());
    m_pageLabel->setString(fmt::format("MAIN {} DE {}", m_index + 1, m_levels.size()).c_str());
}

void GeoBRMainLevelsLayer::showDownloadOverlay() {
    hideDownloadOverlay();
    if (!m_card) return;
    m_downloadOverlay = CCNode::create();
    m_downloadOverlay->setContentSize(m_card->getContentSize());
    m_downloadOverlay->setID("geobr-main-level-download-overlay");
    auto panel = ui::makePanel(m_card->getContentSize(), {8, 25, 58}, 248);
    panel->setPosition(m_card->getContentSize() / 2.f);
    m_downloadOverlay->addChild(panel);
    auto spinner = LoadingSpinner::create(42.f);
    spinner->setPosition({m_card->getContentWidth() / 2.f, m_card->getContentHeight() / 2.f + 19.f});
    m_downloadOverlay->addChild(spinner, 2);
    auto label = ui::makeLabel("Carregando Level...", "bigFont.fnt", .39f, {220, 245, 255});
    label->setPosition({m_card->getContentWidth() / 2.f, m_card->getContentHeight() / 2.f - 23.f});
    m_downloadOverlay->addChild(label, 2);
    m_card->addChild(m_downloadOverlay, 50);
    m_previousButton->setEnabled(false);
    m_nextButton->setEnabled(false);
    if (m_playButton) m_playButton->setEnabled(false);
}

void GeoBRMainLevelsLayer::hideDownloadOverlay() {
    if (m_downloadOverlay) m_downloadOverlay->removeFromParentAndCleanup(true);
    m_downloadOverlay = nullptr;
    if (!m_levels.empty()) {
        m_previousButton->setEnabled(m_index > 0);
        m_nextButton->setEnabled(m_index + 1 < m_levels.size());
        if (m_playButton) m_playButton->setEnabled(true);
    }
}

}
