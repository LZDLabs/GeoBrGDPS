#pragma once
// GeoBR Natal 2026 screen: Candy, daily roulette, calendar (Dec 1 to 25),
// accessory boxes, accessories (equip), the event level, the Batalha Natalina
// (classic: owner opens it, lobby, level vote, whoever beats it first wins;
// bots, BattleGuard), the Natal pass and, for the owner, event tools.
#include "../services/CloudService.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>

namespace geobr {

class NatalHub final : public geode::Popup {
public:
    static NatalHub* create(int tab = 0);
    static void open(int tab = 0);

private:
    int m_tab = 0, m_page = 0;
    bool m_busy = false;
    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCMenu* m_menu = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelBMFont* m_candy = nullptr;
    cocos2d::CCNode* m_wheel = nullptr;
    geode::TextInput* m_inputA = nullptr;
    geode::TextInput* m_inputB = nullptr;
    bool m_arenaLoaded = false, m_pollBusy = false;
    cocos2d::CCLabelBMFont* m_countdown = nullptr;
    std::string m_countdownTarget, m_countdownPrefix;
    int m_bots = 3, m_ownerPage = 0, m_accPage = 0, m_levelPage = 0, m_schedule = 0, m_rankPage = 0;
    bool m_rankLoaded = false;
    matjson::Value m_ranking = matjson::Value::object();
    cocos2d::CCNode* m_pointer = nullptr;
    bool m_shopLoaded = false;
    matjson::Value m_shop = matjson::Value::object();
    std::string m_preview;
    matjson::Value m_config = matjson::Value::object();
    CloudRequest m_action, m_poll, m_upload, m_rankReq;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_download;

    bool init(int tab);
    void status(std::string const& text);
    void render();
    void renderHome();
    void renderBoxes();
    void renderAccessories();
    void renderLevels();
    void renderBattle();
    void renderPass();
    void renderOwner();
    void renderRanking();
    void onRankPage(cocos2d::CCObject*);
    cocos2d::CCMenu* menu();
    CCMenuItemSpriteExtra* button(std::string const& text, int width, char const* bg, cocos2d::SEL_MenuHandler handler, int tag, cocos2d::CCPoint pos, float scale = .32f);
    void call(std::string const& fn, matjson::Value args, std::function<void(matjson::Value const&)> ok);
    void showReward(matjson::Value const& reward, std::string const& title);
    void playLevel(matjson::Value const& row);
    void pollBattle(float);
    void tickCountdown(float);

    void onTab(cocos2d::CCObject*);
    void onPage(cocos2d::CCObject*);
    void onSpin(cocos2d::CCObject*);
    void onDay(cocos2d::CCObject*);
    void onBox(cocos2d::CCObject*);
    void onBuy(cocos2d::CCObject*);
    void onOwnerPage(cocos2d::CCObject*);
    void renderCosmetics(std::string const& kind);
    void onAccPage(cocos2d::CCObject*);
    void onCosmetic(cocos2d::CCObject*);
    void onEquip(cocos2d::CCObject*);
    void onPlay(cocos2d::CCObject*);
    void onEnterArena(cocos2d::CCObject*);
    void onVote(cocos2d::CCObject*);
    void onArenaLeave(cocos2d::CCObject*);
    void onPlayBattle(cocos2d::CCObject*);
    void onSpectate(cocos2d::CCObject*);
    void onScoreboard(cocos2d::CCObject*);
    void onClaimTier(cocos2d::CCObject*);
    void onOwner(cocos2d::CCObject*);
    void onPublishMine(cocos2d::CCObject*);
    void publishLevel(GJGameLevel* level);
};

} // namespace geobr
