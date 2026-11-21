// GeoBR Natal 2026 in the game (GDPS only: main.cpp turns these hooks off on
// the official servers): the Christmas look of the menus (snow, lights, snow
// on the buttons and the ground, Santa hat on the title), the equipped accessory on icons, Candy for event levels and the live
// Natal battle (progress, scoreboard and BattleGuard) while playing.
#include "NatalHub.hpp"
#include "NatalService.hpp"
#include "NatalSpectate.hpp"
#include "NatalTheme.hpp"
#include "../ui/GeoBRVisuals.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/CloudService.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/LevelSelectLayer.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>
#include <Geode/modify/LeaderboardsLayer.hpp>
#include <Geode/modify/MenuGameLayer.hpp>
#include <Geode/binding/GJGroundLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/CCSpritePart.hpp>
#include <Geode/binding/GJSpiderSprite.hpp>
#include <Geode/binding/GJRobotSprite.hpp>
#include <Geode/modify/SimplePlayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/Notification.hpp>

#include <chrono>
#include <cmath>
#include <array>
#include <deque>
#include <random>

using namespace geode::prelude;
using namespace geobr;

namespace {

std::string myAccessory() {
    auto id = natal::equipped();
    return id.empty() && !natal::state().contains("me") ? Mod::get()->getSavedValue<std::string>("natal-equipped") : id;
}

SimplePlayer* firstSimplePlayer(CCNode* node) {
    if (!node) return nullptr;
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        if (auto player = typeinfo_cast<SimplePlayer*>(child)) return player;
        if (auto found = firstSimplePlayer(child)) return found;
    }
    return nullptr;
}

} // namespace

// The Natal screen opens from the GeoBR hub (Criar > GeoBR page). While the
// event is open for the player the GDPS menus get the Christmas look.
class $modify(GeoBRNatalMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        natal::theme::mainMenu(this);
        natal::startBattleWatch();
        WeakRef<MenuLayer> weak = this;
        natal::refresh([weak](bool, std::string const&) { if (auto self = weak.lock()) natal::theme::mainMenu(self); });
        return true;
    }
};

// Red sky and snowy ground behind the main menu (GD recolours them every tick).
class $modify(GeoBRNatalMenuGame, MenuGameLayer) {
    void tint() {
        if (!natal::theme::on()) return;
        if (m_backgroundSprite) m_backgroundSprite->setColor(natal::theme::Red);
        if (!m_groundLayer) return;
        auto paint = [](CCNode* node, ccColor3B color) {
            if (!node) return;
            if (auto rgba = typeinfo_cast<CCRGBAProtocol*>(node)) rgba->setColor(color);
            for (auto child : CCArrayExt<CCNode*>(node->getChildren())) if (auto rgba = typeinfo_cast<CCRGBAProtocol*>(child)) rgba->setColor(color);
        };
        paint(m_groundLayer->m_ground1Sprite, {214, 230, 255});
        paint(m_groundLayer->m_ground2Sprite, {236, 244, 255});
    }
    bool init() {
        if (!MenuGameLayer::init()) return false;
        tint();
        return true;
    }
    void updateColor(float dt) {
        MenuGameLayer::updateColor(dt);
        tint();
    }
};

// Pick one of your accessories (works after the event too).
class AccessoryPicker final : public Popup {
    std::function<void()> m_done;
    CCMenu* m_grid = nullptr;
    CCLabelBMFont* m_info = nullptr;
    CloudRequest m_request;
    int m_page = 0;
    bool m_busy = false;

public:
    static AccessoryPicker* create(std::function<void()> done) {
        auto p = new AccessoryPicker;
        p->m_done = std::move(done);
        if (p->init()) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    bool init() {
        if (!Popup::init(380.f, 250.f)) return false;
        setTitle("Seus acessorios de Natal", "goldFont.fnt", .7f, 18);
        m_info = CCLabelBMFont::create("", "chatFont.fnt");
        m_info->setScale(.5f);
        m_info->setPosition({190.f, 18.f});
        m_mainLayer->addChild(m_info, 5);
        render();
        WeakRef<AccessoryPicker> weak = this;
        natal::refresh([weak](bool, std::string const&) { if (auto self = weak.lock()) self->render(); });
        return true;
    }
    std::vector<matjson::Value> owned() {
        std::vector<matjson::Value> out;
        auto inv = natal::state()["inventory"];
        auto all = natal::accessories();
        if (!all.isArray()) return out;
        for (auto const& a : all.asArray().unwrap()) {
            auto id = jsonutil::stringOr(a, "id");
            if (inv.isArray()) for (auto const& o : inv.asArray().unwrap()) if (o.asString().unwrapOr("") == id) { out.push_back(a); break; }
        }
        return out;
    }
    void render() {
        if (m_grid) m_grid->removeFromParent();
        m_grid = CCMenu::create();
        m_grid->setPosition({0, 0});
        m_mainLayer->addChild(m_grid, 4);
        auto list = owned();
        auto equipped = natal::equipped();
        if (list.empty()) m_info->setString("Voce ainda nao tem acessorios. Ganhe no evento de Natal!");
        else m_info->setString("Toque para usar. O acessorio aparece no seu icone e no perfil.");
        m_info->limitLabelWidth(350.f, .5f, .2f);
        int perPage = 10, pages = std::max<int>(1, (static_cast<int>(list.size()) + 1 + perPage - 1) / perPage);
        m_page = std::clamp(m_page, 0, pages - 1);
        for (int i = 0; i < perPage; ++i) {
            int index = m_page * perPage + i - 1;  // -1 = "Nenhum"
            if (index >= static_cast<int>(list.size())) break;
            CCPoint at{70.f + (i % 5) * 60.f, 170.f - (i / 5) * 62.f};
            CCNode* look;
            if (index < 0) {
                auto l = CCLabelBMFont::create("Nenhum", "bigFont.fnt");
                l->setScale(.3f);
                look = l;
            } else {
                auto art = CCSprite::create(Mod::get()->expandSpriteName(jsonutil::stringOr(list[index], "sprite")).data());
                if (!art) continue;
                art->setScale(40.f / std::max(1.f, std::max(art->getContentWidth(), art->getContentHeight())));
                look = art;
            }
            auto item = CCMenuItemSpriteExtra::create(look, this, menu_selector(AccessoryPicker::onPick));
            item->setTag(index);
            item->setPosition(at);
            m_grid->addChild(item);
            bool on = index < 0 ? equipped.empty() : jsonutil::stringOr(list[index], "id") == equipped;
            if (on) {
                auto mark = CCLabelBMFont::create("USANDO", "bigFont.fnt");
                mark->setScale(.2f);
                mark->setColor({150, 255, 160});
                mark->setPosition({at.x, at.y - 26.f});
                m_grid->addChild(mark);
            }
        }
        if (pages > 1) {
            for (int dir : {-1, 1}) {
                auto s = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
                s->setScale(.5f);
                if (dir > 0) s->setFlipX(true);
                auto b = CCMenuItemSpriteExtra::create(s, this, menu_selector(AccessoryPicker::onPage));
                b->setTag(dir);
                b->setPosition({dir < 0 ? 24.f : 356.f, 140.f});
                m_grid->addChild(b);
            }
        }
    }
    void onPage(CCObject* sender) { m_page += sender->getTag(); render(); }
    void onPick(CCObject* sender) {
        if (m_busy) return;
        int index = sender->getTag();
        auto list = owned();
        matjson::Value id = nullptr;
        if (index >= 0 && index < static_cast<int>(list.size())) id = jsonutil::stringOr(list[index], "id");
        m_busy = true;
        m_info->setString("...");
        WeakRef<AccessoryPicker> weak = this;
        m_request.rpc("geobr_natal_equip", matjson::makeObject({{"p_accessory", id}}), [weak](auto r) {
            auto self = weak.lock();
            if (!self) return;
            self->m_busy = false;
            if (!r.success) { self->m_info->setString(r.error.c_str()); return; }
            natal::setState(r.value);
            self->render();
            if (self->m_done) self->m_done();
        });
    }
};

// Every preview icon remembers which mode it shows (cube, robot...), so the
// accessory goes on the right place (head of the robot/spider, hidden on ships).
class $modify(GeoBRNatalSimplePlayer, SimplePlayer) {
    void updatePlayerFrame(int id, IconType type) {
        SimplePlayer::updatePlayerFrame(id, type);
        setUserObject("geobr.icon-type", CCInteger::create(static_cast<int>(type)));
        if (auto acc = typeinfo_cast<CCString*>(getUserObject("geobr.natal-accessory"))) natal::attach(this, acc->getCString());
    }
};

class $modify(GeoBRNatalGarage, GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init()) return false;
        natal::attach(m_playerObject, myAccessory());
        natal::theme::snow(this);
        // Button: choose your Christmas accessory here, like an icon.
        if (auto icon = CCSprite::create(Mod::get()->expandSpriteName("natal_acc_gorro_papai_noel.png").data())) {
            icon->setScale(26.f / std::max(1.f, icon->getContentHeight()));
            auto sprite = CircleButtonSprite::create(icon, CircleBaseColor::Green, CircleBaseSize::Small);
            auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(GeoBRNatalGarage::onAccessories));
            button->setID("geobr-natal-accessories-button");
            auto menu = CCMenu::create();
            menu->setID("geobr-natal-accessories-menu");
            auto win = CCDirector::sharedDirector()->getWinSize();
            menu->setPosition({28.f, win.height / 2.f + 10.f});
            menu->addChild(button);
            addChild(menu, 20);
        }
        return true;
    }
    void onAccessories(CCObject*) {
        WeakRef<GJGarageLayer> weak = this;
        if (auto p = AccessoryPicker::create([weak] { if (auto self = weak.lock()) natal::attach(self->m_playerObject, natal::equipped()); })) p->show();
    }
};

// Other GDPS menus get the snow and the lights (not the editor, not gameplay).
namespace {
void christmasMenu(CCLayer* layer, ccColor3B color) {
    natal::theme::recolorBackground(layer, color);
    natal::theme::frame(layer, false);
    natal::theme::lights(layer);
    natal::theme::snow(layer);
}
} // namespace

class $modify(GeoBRNatalCreator, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;
        christmasMenu(this, natal::theme::Red);
        return true;
    }
};

class $modify(GeoBRNatalLevelSelect, LevelSelectLayer) {
    bool init(int page) {
        if (!LevelSelectLayer::init(page)) return false;
        natal::theme::frame(this, false);
        natal::theme::lights(this);
        natal::theme::snow(this);
        return true;
    }
};

class $modify(GeoBRNatalBrowser, LevelBrowserLayer) {
    bool init(GJSearchObject* search) {
        if (!LevelBrowserLayer::init(search)) return false;
        christmasMenu(this, natal::theme::Green);
        return true;
    }
};

class $modify(GeoBRNatalSearch, LevelSearchLayer) {
    bool init(int type) {
        if (!LevelSearchLayer::init(type)) return false;
        christmasMenu(this, natal::theme::Green);
        return true;
    }
};

class $modify(GeoBRNatalLeaderboards, LeaderboardsLayer) {
    bool init(LeaderboardType type, LeaderboardStat stat) {
        if (!LeaderboardsLayer::init(type, stat)) return false;
        christmasMenu(this, natal::theme::Red);
        return true;
    }
};

// Profiles: the equipped accessory on the icon, the Christmas theme behind the
// card and the frame (antlers, wreath, lights, candy cane, snow) around it.
class $modify(GeoBRNatalProfile, ProfilePage) {
    struct Fields { CloudRequest request; };

    CCNode* panel() {
        CCNode* best = nullptr;
        float largest = 0;
        for (auto child : CCArrayExt<CCNode*>(m_mainLayer->getChildren())) {
            if (typeinfo_cast<CCMenu*>(child) || typeinfo_cast<CCLayer*>(child)) continue;
            auto size = child->getScaledContentSize();
            float area = size.width * size.height;
            if (size.width >= 300 && size.height >= 200 && area > largest) { best = child; largest = area; }
        }
        return best;
    }

    void applyNatal(std::string const& accessory, std::string const& theme, std::string const& frame) {
        natal::attach(firstSimplePlayer(m_mainLayer), accessory);
        auto p = panel();
        if (!p) return;
        if (auto old = p->getChildByID("geobr-natal-profile-theme")) old->removeFromParent();
        if (!theme.empty()) {
            auto size = p->getContentSize();
            float inset = 6 / std::max(.1f, p->getScaleX());
            auto bg = visual::theme({size.width - inset * 2, size.height - inset * 2}, theme);
            bg->setPosition({inset, inset});
            bg->setID("geobr-natal-profile-theme");
            p->addChild(bg, 2);
            if (m_usernameLabel) m_usernameLabel->setColor(visual::palette(theme).accent);
        }
        natal::theme::profileFrame(p, frame);
    }

    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);
        if (!score || score->m_accountID <= 0) return;
        auto ids = matjson::Value::array();
        ids.push(score->m_accountID);
        Ref<ProfilePage> keep = this;
        m_fields->request.rpc("geobr_natal_equipped", matjson::makeObject({{"p_accounts", ids}}), [keep](auto r) {
            if (!r.success || !r.value.isArray() || !keep->getParent()) return;
            auto rows = r.value.asArray().unwrap();
            if (rows.empty()) return;
            auto const& row = rows[0];
            auto str = [&](char const* key) { auto v = row.get(key); return v && v.unwrap().isString() ? v.unwrap().asString().unwrap() : std::string(); };
            static_cast<GeoBRNatalProfile*>(keep.data())->applyNatal(str("accessory"), str("theme"), str("frame"));
        });
    }
};

// The battle level waits frozen at the start (no physics) while the others load it.
static PlayLayer* s_holdLayer = nullptr;

namespace {
double steadySeconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
// Game mode of an icon as a number for the live positions (0 cube .. 7 swing).
int modeOf(PlayerObject* p) {
    if (p->m_isShip) return 1;
    if (p->m_isBall) return 2;
    if (p->m_isBird) return 3;
    if (p->m_isDart) return 4;
    if (p->m_isRobot) return 5;
    if (p->m_isSpider) return 6;
    if (p->m_isSwing) return 7;
    return 0;
}
IconType iconOfMode(int m) {
    static constexpr IconType types[] = {IconType::Cube, IconType::Ship, IconType::Ball, IconType::Ufo, IconType::Wave, IconType::Robot, IconType::Spider, IconType::Swing};
    return types[std::clamp(m, 0, 7)];
}
} // namespace

class $modify(GeoBRNatalHold, GJBaseGameLayer) {
    void update(float dt) {
        if (s_holdLayer && static_cast<GJBaseGameLayer*>(s_holdLayer) == this) return;
        GJBaseGameLayer::update(dt);
    }
};

class $modify(GeoBRNatalPlay, PlayLayer) {
    struct Fields {
        std::int64_t natal = 0;
        bool battle = false, completed = false;
        float elapsed = 0, attempt = 0, send = 0, guard = 0, tilt = 0, spin = 0, baseScale = 1;
        CCSprite* band = nullptr;
        bool followerTried = false;
        CCSprite* follower = nullptr;
        CCPoint offset;
        // Battle: the level waits frozen at the start until everyone loaded it, then 3-2-1.
        bool hold = false, readySent = false, pollBusy = false, speedReported = false, battleLevel = false;
        std::chrono::steady_clock::time_point wallStart = std::chrono::steady_clock::now();
        float pollT = 0, cycleT = 0;
        size_t cycle = 0;
        CCNode* corner = nullptr;
        CCLabelBMFont* vsName = nullptr;
        CCLabelBMFont* vsPct = nullptr;
        CCLabelBMFont* vsPlace = nullptr;
        CCLayerColor* vsBar = nullptr;
        CCLabelBMFont* center = nullptr;
        CloudRequest reward, progress, guardCheck, ready;
        // Live positions: 10 samples a second, sent every second (for whoever spectates).
        matjson::Value samples = matjson::Value::array();
        float sampleT = 0, liveT = 0, batchT = 0;
        bool liveBusy = false;
        std::vector<std::array<float, 3>> path, bestPath;   // x, y, mode: this attempt / the furthest one (route for the bot)
        float pathMax = 0, bestMax = 0;
        // Spectate: the camera follows another player's icon (after you beat it, or from the hub).
        struct Sample { double at; float x, y, r; int m; };
        bool spectating = false, spectateOnly = false, spectateAfterReset = false, watchBusy = false;
        std::string targetKey;
        std::deque<Sample> buf;
        double lastBatch = 0;
        float watchT = 10, botX = 0;
        matjson::Value watch = matjson::Value::object();
        SimplePlayer* ghost = nullptr;
        int ghostMode = -1;
        CCLabelBMFont* ghostName = nullptr;
        CCNode* specBar = nullptr;
        CCLabelBMFont* specLabel = nullptr;
        CloudRequest live, watchReq;
    };

    bool init(GJGameLevel* level, bool replay, bool empty) {
        if (!PlayLayer::init(level, replay, empty)) return false;
        auto f = m_fields.self();
        f->natal = level ? natal::eventLevelFromLocal(level->m_levelID.value()) : 0;
        auto const& b = natal::battle();
        f->battle = f->natal > 0 && jsonutil::stringOr(b, "state") == "running" && jsonutil::boolOr(b, "joined", false) && b.get("level") &&
            b["level"].isObject() && jsonutil::integerOr(b["level"], "id") == f->natal;
        s_holdLayer = nullptr;
        f->battleLevel = f->battle;
        if (f->battle) {
            buildCorner();
            f->hold = jsonutil::boolOr(b, "loading", false) || natal::secondsUntil(jsonutil::stringOr(b, "starts_at")) > 0;
            if (f->hold) s_holdLayer = this;
            schedule(schedule_selector(GeoBRNatalPlay::battleTick), .25f);
            runGuard();
        }
        // Opened from the hub to watch: the camera follows the others from the start.
        if (natal::takeSpectateRequest() && f->natal > 0) {
            f->spectateOnly = true;
            if (!f->battle) { buildCorner(); schedule(schedule_selector(GeoBRNatalPlay::battleTick), .25f); }
        }
        return true;
    }

    // No practice mode in the battle level while the battle is on.
    void togglePracticeMode(bool practice) {
        auto f = m_fields.self();
        auto const& b = natal::battle();
        bool live = f->battleLevel && jsonutil::stringOr(b, "state") == "running" && b.get("level") && b["level"].isObject()
            && jsonutil::integerOr(b["level"], "id") == f->natal;
        if (practice && live) {
            Notification::create("Modo pratica bloqueado na Batalha Natalina", NotificationIcon::Warning, 2.5f)->show();
            return;
        }
        PlayLayer::togglePracticeMode(practice);
    }

    void onQuit() {
        if (s_holdLayer == this) s_holdLayer = nullptr;
        PlayLayer::onQuit();
    }

    // Top-left corner: who you are racing right now (changes every 3 s) and their %.
    void buildCorner() {
        auto f = m_fields.self();
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto box = CCLayerColor::create({0, 0, 0, 130}, 150.f, 44.f);
        box->setPosition({6.f, win.height - 50.f});
        box->setID("geobr-natal-battle-corner");
        addChild(box, 1000);
        f->corner = box;
        auto title = CCLabelBMFont::create("BATALHA", "goldFont.fnt");
        title->setScale(.32f);
        title->setAnchorPoint({0.f, .5f});
        title->setPosition({6.f, 36.f});
        box->addChild(title);
        f->vsName = CCLabelBMFont::create("Procurando rivais...", "chatFont.fnt");
        f->vsName->setScale(.5f);
        f->vsName->setAnchorPoint({0.f, .5f});
        f->vsName->setPosition({6.f, 22.f});
        box->addChild(f->vsName);
        f->vsPct = CCLabelBMFont::create("", "bigFont.fnt");
        f->vsPct->setScale(.32f);
        f->vsPct->setAnchorPoint({1.f, .5f});
        f->vsPct->setPosition({144.f, 22.f});
        box->addChild(f->vsPct);
        auto track = CCLayerColor::create({60, 60, 60, 220}, 138.f, 5.f);
        track->setPosition({6.f, 9.f});
        box->addChild(track);
        f->vsBar = CCLayerColor::create({250, 195, 45, 255}, 1.f, 5.f);
        f->vsBar->setPosition({6.f, 9.f});
        box->addChild(f->vsBar);
        f->vsPlace = CCLabelBMFont::create("", "chatFont.fnt");
        f->vsPlace->setScale(.4f);
        f->vsPlace->setAnchorPoint({1.f, .5f});
        f->vsPlace->setPosition({144.f, 36.f});
        box->addChild(f->vsPlace);
        auto center = CCLabelBMFont::create("", "bigFont.fnt");
        center->setPosition(win / 2.f + CCPoint{0.f, 40.f});
        center->setID("geobr-natal-battle-center");
        addChild(center, 1001);
        f->center = center;
    }

    void showCenter(std::string const& text, float scale) {
        auto f = m_fields.self();
        if (!f->center) return;
        f->center->setString(text.c_str());
        f->center->setScale(scale);
        f->center->setVisible(!text.empty());
    }

    // Opponents from the last answer, best first; the corner shows one at a time.
    void updateCorner(float dt) {
        auto f = m_fields.self();
        if (!f->corner) return;
        auto const& arena = natal::battle();
        auto players = arena["players"];
        if (!players.isArray()) return;
        std::vector<matjson::Value> rivals;
        int place = 0, n = 0;
        for (auto const& p : players.asArray().unwrap()) {
            ++n;
            if (jsonutil::boolOr(p, "me", false)) { place = n; continue; }
            if (!jsonutil::boolOr(p, "disqualified", false)) rivals.push_back(p);
        }
        f->vsPlace->setString(place > 0 ? fmt::format("Voce: {}o de {}", place, n).c_str() : "");
        f->cycleT += dt;
        if (f->cycleT >= 3.f) { f->cycleT = 0; ++f->cycle; }
        if (rivals.empty()) {
            f->vsName->setString("Sem rivais por enquanto");
            f->vsPct->setString("");
            f->vsBar->setContentSize({0.f, 5.f});
            return;
        }
        auto const& r = rivals[f->cycle % rivals.size()];
        bool done = jsonutil::boolOr(r, "finished", false);
        int pct = done ? 100 : static_cast<int>(jsonutil::integerOr(r, "percent"));
        auto name = jsonutil::stringOr(r, "name");
        if (name.size() > 16) name = name.substr(0, 16);
        f->vsName->setString(("VS " + name).c_str());
        f->vsPct->setString(done ? "PASSOU!" : fmt::format("{}%", pct).c_str());
        f->vsBar->setContentSize({std::max(1.f, 1.38f * pct), 5.f});
    }

    // Beat the level: the server checks the mods again, the minimum time of the
    // level and that the game did not run faster than real time (anti-cheat).
    void sendFinish() {
        auto f = m_fields.self();
        auto mods = matjson::Value::array();
        for (auto const& id : natal::enabledMods()) mods.push(id);
        auto wall = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - f->wallStart).count();
        Ref<PlayLayer> keep = this;
        f->progress.rpc("geobr_natal_arena_finish", matjson::makeObject({
            {"p_ms", static_cast<int>(f->attempt * 1000.f)}, {"p_wall_ms", static_cast<int>(wall)}, {"p_mods", mods},
        }), [keep](auto r) {
            if (!r.success || !r.value.contains("arena") || !r.value["arena"].isObject()) return;
            natal::setBattle(r.value["arena"]);
            auto self = static_cast<GeoBRNatalPlay*>(keep.data());
            auto why = jsonutil::stringOr(r.value, "rejected");
            self->m_fields->battle = false;
            if (!why.empty()) {
                // A warning on the screen, not an error box in the middle of the run.
                self->showCenter("BattleGuard: desclassificado (" + why + ")", .45f);
                Notification::create("BattleGuard: " + why, NotificationIcon::Error, 4.f)->show();
                return;
            }
            auto const& arena = r.value["arena"];
            if (!self->getParent()) return;
            bool running = jsonutil::stringOr(arena, "state") == "running";
            int place = 0, prize = 0, passed = 0;
            auto players = arena["players"];
            if (players.isArray()) for (auto const& p : players.asArray().unwrap()) {
                if (jsonutil::boolOr(p, "finished", false)) ++passed;
                if (!jsonutil::boolOr(p, "me", false)) continue;
                place = static_cast<int>(jsonutil::integerOr(p, "place"));
                prize = static_cast<int>(jsonutil::integerOr(p, "prize"));
                if (running) place = passed;
            }
            std::string result = running
                ? fmt::format("Voce passou em <cy>{}o lugar</c>! Os Candy saem quando a batalha acabar (vem um aviso).", std::max(1, place))
                : prize > 0 ? fmt::format("Voce ficou em <cy>{}o lugar</c> e ganhou <cg>{} Candy</c>!", std::max(1, place), prize)
                            : fmt::format("Voce ficou em <cy>{}o lugar</c>.", std::max(1, place));
            self->showCenter(running ? fmt::format("Voce passou! ({}o)", std::max(1, place)) : fmt::format("+{} Candy", prize), .6f);
            Ref<PlayLayer> layer = self;
            self->runAction(CCSequence::create(CCDelayTime::create(2.5f), CallFuncExt::create([layer, result, running] {
                natal::offerAfterFinish(result, running, [layer] {
                    static_cast<GeoBRNatalPlay*>(layer.data())->m_fields->spectateAfterReset = true;
                    natal::replayBattleLevel();
                });
            }), nullptr));
        });
    }

    void battleTick(float dt) {
        auto f = m_fields.self();
        if (f->spectateOnly && !f->spectating && !f->hold) startSpectate();
        if (f->spectating) { pollWatch(dt); updateCorner(dt); return; }
        // Speedhack: the game clock must run at normal speed during the battle.
        if (f->battle && !f->speedReported && std::abs(CCDirector::sharedDirector()->getScheduler()->getTimeScale() - 1.f) > .01f) {
            f->speedReported = true;
            runGuard();
        }
        if (!f->battle && !f->hold) { updateCorner(dt); return; }
        auto const& arena = natal::battle();
        if (f->hold) {
            FMODAudioEngine::sharedEngine()->stopAllMusic(true);
            // Tell the server this game has the level loaded, then wait for everyone.
            if (!f->readySent && !f->pollBusy) {
                f->pollBusy = true;
                Ref<PlayLayer> keep = this;
                f->ready.rpc("geobr_natal_arena_ready", matjson::Value::object(), [keep](auto r) {
                    auto self = static_cast<GeoBRNatalPlay*>(keep.data());
                    self->m_fields->pollBusy = false;
                    if (!r.success || !r.value.contains("arena")) return;
                    self->m_fields->readySent = true;
                    natal::setBattle(r.value["arena"]);
                });
            } else if (f->readySent && !f->pollBusy && (f->pollT += dt) >= 1.f) {
                f->pollT = 0;
                f->pollBusy = true;
                Ref<PlayLayer> keep = this;
                f->ready.rpc("geobr_natal_arena_state", matjson::Value::object(), [keep](auto r) {
                    auto self = static_cast<GeoBRNatalPlay*>(keep.data());
                    self->m_fields->pollBusy = false;
                    if (r.success && r.value.contains("arena")) natal::setBattle(r.value["arena"]);
                });
            }
            auto state = jsonutil::stringOr(arena, "state");
            if (state != "running") {
                f->hold = false;
                if (s_holdLayer == this) s_holdLayer = nullptr;
                showCenter(state == "cancelled" ? "Batalha cancelada" : "", .6f);
            } else if (jsonutil::boolOr(arena, "loading", false)) {
                showCenter(fmt::format("Esperando todos carregarem ({}/{})", jsonutil::integerOr(arena, "ready"), jsonutil::integerOr(arena, "humans")), .5f);
            } else {
                double left = natal::secondsUntil(jsonutil::stringOr(arena, "starts_at"));
                if (left > 0) showCenter(std::to_string(static_cast<int>(std::ceil(left))), 1.6f);
                else {
                    // GO: everyone starts the attempt from the beginning at the same time.
                    f->hold = false;
                    if (s_holdLayer == this) s_holdLayer = nullptr;
                    showCenter("VAI!", 1.4f);
                    if (f->center) f->center->runAction(CCSequence::create(CCDelayTime::create(.8f), CCHide::create(), nullptr));
                    resetLevel();
                    startMusic();
                }
            }
        }
        updateCorner(dt);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        auto f = m_fields.self();
        f->attempt = 0;
        f->wallStart = std::chrono::steady_clock::now();
        // The furthest attempt is the level's route (the bot's icon follows it when watched).
        if (f->pathMax > f->bestMax) { f->bestMax = f->pathMax; f->bestPath = f->path; }
        f->path.clear();
        f->pathMax = 0;
        if (f->spectateAfterReset) { f->spectateAfterReset = false; startSpectate(); }
    }

    // While watching, our hidden icon cannot die or finish.
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (m_fields->spectating) return;
        PlayLayer::destroyPlayer(player, object);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto f = m_fields.self();
        if (f->spectating) { driveSpectate(dt); return; }
        followPlayer(dt);
        if (f->natal <= 0) return;
        if (!m_isPaused) { f->elapsed += dt; f->attempt += dt; }
        if (!f->battle || m_isPracticeMode || m_isTestMode) return;  // practice runs do not count in the battle
        // Live positions (10 a second) and %, sent every second.
        f->sampleT += dt;
        f->batchT += dt * 1000.f;
        if (f->sampleT >= .1f && m_player1) {
            f->sampleT = 0;
            auto p = m_player1;
            int m = modeOf(p);
            auto pos = p->getPosition();
            auto sample = matjson::Value::array();
            sample.push(static_cast<int>(f->batchT));
            sample.push(static_cast<int>(std::lround(pos.x)));
            sample.push(static_cast<int>(std::lround(pos.y)));
            sample.push(static_cast<int>(std::lround(p->getRotation())));
            sample.push(m);
            sample.push(p->m_isDead ? 1 : 0);
            if (f->samples.size() < 30) f->samples.push(sample);
            f->path.push_back({pos.x, pos.y, static_cast<float>(m)});
            f->pathMax = std::max(f->pathMax, pos.x);
        }
        f->liveT += dt;
        f->guard += dt;
        if (f->liveT >= 1.f && !f->liveBusy) { f->liveT = 0; sendLive(); }
        if (f->guard >= 30.f) { f->guard = 0; runGuard(); }
    }

    void sendLive() {
        auto f = m_fields.self();
        auto trail = f->samples;
        f->samples = matjson::Value::array();
        f->batchT = 0;
        f->liveBusy = true;
        int pct = getCurrentPercentInt();
        Ref<PlayLayer> keep = this;
        f->live.rpc("geobr_natal_arena_live", matjson::makeObject({{"p_percent", pct}, {"p_cur", pct}, {"p_trail", trail}}), [keep](auto r) {
            auto self = static_cast<GeoBRNatalPlay*>(keep.data());
            self->m_fields->liveBusy = false;
            if (!r.success || !r.value.contains("arena") || !r.value["arena"].isObject()) return;
            natal::setBattle(r.value["arena"]);
        });
    }

    // ------------------------------------------------------------ spectate
    void startSpectate() {
        auto f = m_fields.self();
        auto p = m_player1;
        if (f->spectating || !p) return;
        f->spectating = true;
        f->buf.clear();
        f->lastBatch = 0;
        f->watchT = 10;
        auto parent = p->getParent();
        if (parent && !typeinfo_cast<CCSpriteBatchNode*>(parent)) {
            f->ghost = SimplePlayer::create(1);
            f->ghost->updatePlayerFrame(1, IconType::Cube);
            f->ghost->setID("geobr-natal-ghost");
            f->ghostMode = 0;
            parent->addChild(f->ghost, p->getZOrder() + 2);
            f->ghostName = CCLabelBMFont::create("", "goldFont.fnt");
            f->ghostName->setScale(.45f);
            parent->addChild(f->ghostName, p->getZOrder() + 3);
        }
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto bar = CCNode::create();
        bar->setID("geobr-natal-spectate-bar");
        bar->setPosition({win.width / 2.f, win.height - 40.f});
        addChild(bar, 1002);
        auto bg = CCLayerColor::create({0, 0, 0, 150}, 280.f, 46.f);
        bg->setPosition({-140.f, -32.f});
        bar->addChild(bg);
        f->specLabel = CCLabelBMFont::create("Espectando...", "chatFont.fnt");
        f->specLabel->setScale(.55f);
        f->specLabel->setPosition({0.f, 2.f});
        bar->addChild(f->specLabel);
        auto menu = CCMenu::create();
        menu->setPosition({0.f, -19.f});
        auto make = [&](char const* text, SEL_MenuHandler handler, float x, float width) {
            auto spr = ButtonSprite::create(text, static_cast<int>(width), true, "bigFont.fnt", "GJ_button_04.png", 24.f, .5f);
            spr->setScale(.6f);
            auto b = CCMenuItemSpriteExtra::create(spr, this, handler);
            b->setPosition({x, 0.f});
            menu->addChild(b);
        };
        make("<", menu_selector(GeoBRNatalPlay::onSpecPrev), -110.f, 30.f);
        make(">", menu_selector(GeoBRNatalPlay::onSpecNext), 110.f, 30.f);
        make(f->spectateOnly ? "Sair" : "Voltar a jogar", menu_selector(GeoBRNatalPlay::onSpecExit), 0.f, 130.f);
        bar->addChild(menu);
        f->specBar = bar;
        p->setVisible(false);
    }

    void stopSpectate() {
        auto f = m_fields.self();
        if (!f->spectating) return;
        f->spectating = false;
        if (f->ghost) { f->ghost->removeFromParent(); f->ghost = nullptr; }
        if (f->ghostName) { f->ghostName->removeFromParent(); f->ghostName = nullptr; }
        if (f->specBar) { f->specBar->removeFromParent(); f->specBar = nullptr; }
        if (m_player1) m_player1->setVisible(true);
        resetLevel();
    }

    std::vector<matjson::Value> rivals() {
        std::vector<matjson::Value> out;
        auto players = m_fields->watch["players"];
        if (players.isArray()) for (auto const& p : players.asArray().unwrap()) if (!jsonutil::boolOr(p, "me", false)) out.push_back(p);
        return out;
    }

    void switchTarget(int step) {
        auto f = m_fields.self();
        auto list = rivals();
        if (list.empty()) return;
        int index = 0;
        for (size_t i = 0; i < list.size(); ++i) if (jsonutil::stringOr(list[i], "key") == f->targetKey) index = static_cast<int>(i);
        index = (index + step + static_cast<int>(list.size())) % static_cast<int>(list.size());
        f->targetKey = jsonutil::stringOr(list[index], "key");
        f->buf.clear();
        f->lastBatch = 0;
        f->botX = 0;
        f->watchT = 10;
    }
    void onSpecPrev(CCObject*) { switchTarget(-1); }
    void onSpecNext(CCObject*) { switchTarget(1); }
    void onSpecExit(CCObject*) {
        if (m_fields->spectateOnly) { onQuit(); return; }
        stopSpectate();
    }

    // Every second: the others' last second of positions (and the bots' current %).
    void pollWatch(float dt) {
        auto f = m_fields.self();
        f->watchT += dt;
        if (f->watchBusy || f->watchT < 1.f) return;
        f->watchT = 0;
        f->watchBusy = true;
        Ref<PlayLayer> keep = this;
        f->watchReq.rpc("geobr_natal_arena_watch", matjson::Value::object(), [keep](auto r) {
            auto self = static_cast<GeoBRNatalPlay*>(keep.data());
            auto f = self->m_fields.self();
            f->watchBusy = false;
            if (!r.success || !r.value.isObject()) return;
            f->watch = r.value;
            auto list = self->rivals();
            if (list.empty()) { if (f->specLabel) f->specLabel->setString("Ninguem para espectar agora"); return; }
            matjson::Value const* target = nullptr;
            for (auto const& p : list) if (jsonutil::stringOr(p, "key") == f->targetKey) target = &p;
            if (!target) { target = &list.front(); f->targetKey = jsonutil::stringOr(*target, "key"); f->buf.clear(); f->lastBatch = 0; }
            bool bot = jsonutil::boolOr(*target, "bot", false), done = jsonutil::boolOr(*target, "finished", false);
            auto name = jsonutil::stringOr(*target, "name");
            int pct = static_cast<int>(jsonutil::integerOr(*target, bot ? "cur" : "percent"));
            if (f->specLabel) f->specLabel->setString(fmt::format("Espectando {}  {}", name, done ? std::string("PASSOU!") : fmt::format("{}%", pct)).c_str());
            if (f->ghostName) f->ghostName->setString(name.c_str());
            if (bot) return;
            // Positions: each sample's time on this device's clock.
            auto trail = (*target)["trail"];
            if (!trail.isArray() || trail.size() == 0) return;
            double batchEnd = steadySeconds() - jsonutil::integerOr(*target, "age") / 1000.0;
            if (std::abs(batchEnd - f->lastBatch) < .3) return;
            f->lastBatch = batchEnd;
            auto items = trail.asArray().unwrap();
            double lastT = items.back()[0].asInt().unwrapOr(0);
            for (auto const& s : items) {
                if (!s.isArray() || s.size() < 5) continue;
                double at = batchEnd - (lastT - s[0].asInt().unwrapOr(0)) / 1000.0;
                if (!f->buf.empty() && at <= f->buf.back().at) continue;
                f->buf.push_back({at, static_cast<float>(s[1].asInt().unwrapOr(0)), static_cast<float>(s[2].asInt().unwrapOr(0)),
                                  static_cast<float>(s[3].asInt().unwrapOr(0)), static_cast<int>(s[4].asInt().unwrapOr(0))});
            }
            while (f->buf.size() > 2 && f->buf.front().at < steadySeconds() - 12.0) f->buf.pop_front();
        });
    }

    // Every frame: the watched icon moves (1.6 s behind, smooth); our hidden icon
    // goes along with it so the camera, colours and triggers follow that player.
    void driveSpectate(float dt) {
        auto f = m_fields.self();
        auto p = m_player1;
        if (!p || !f->ghost) return;
        matjson::Value const* target = nullptr;
        auto players = f->watch["players"];
        if (players.isArray()) for (auto const& t : players.asArray().unwrap()) if (jsonutil::stringOr(t, "key") == f->targetKey) target = &t;
        if (!target) return;
        CCPoint pos;
        float rot = 0;
        int mode = 0;
        if (jsonutil::boolOr(*target, "bot", false)) {
            // The bot plays the level's real route (the furthest attempt of this game) at its current %.
            float tx = m_levelLength * static_cast<float>(jsonutil::integerOr(*target, "cur")) / 100.f;
            if (tx < f->botX - 60.f) f->botX = tx;
            else f->botX += (tx - f->botX) * std::min(1.f, dt * 2.5f);
            auto const& route = f->bestPath.size() > f->path.size() ? f->bestPath : f->path;
            float y = p->getPositionY();
            for (size_t i = 1; i < route.size(); ++i) {
                if (route[i][0] < f->botX) continue;
                auto const& a = route[i - 1];
                auto const& b = route[i];
                float k = b[0] > a[0] ? (f->botX - a[0]) / (b[0] - a[0]) : 0.f;
                y = a[1] + (b[1] - a[1]) * std::clamp(k, 0.f, 1.f);
                mode = static_cast<int>(a[2]);
                break;
            }
            pos = CCPoint(f->botX, y);
        } else {
            if (f->buf.empty()) return;
            double at = steadySeconds() - 1.6;
            auto const* a = &f->buf.front();
            auto const* b = a;
            for (size_t i = 1; i < f->buf.size(); ++i) {
                if (f->buf[i].at >= at) { a = &f->buf[i - 1]; b = &f->buf[i]; break; }
                a = b = &f->buf[i];
            }
            float k = b->at > a->at ? static_cast<float>((at - a->at) / (b->at - a->at)) : 1.f;
            k = std::clamp(k, 0.f, 1.f);
            if (b->x < a->x - 100.f) k = 1.f;   // restarted: jump back
            pos = CCPoint(a->x + (b->x - a->x) * k, a->y + (b->y - a->y) * k);
            rot = a->r + (b->r - a->r) * k;
            mode = k < .5f ? a->m : b->m;
        }
        f->ghost->setPosition(pos);
        f->ghost->setRotation(rot);
        if (mode != f->ghostMode) { f->ghostMode = mode; f->ghost->updatePlayerFrame(1, iconOfMode(mode)); }
        if (f->ghostName) f->ghostName->setPosition(pos + CCPoint{0.f, 26.f});
        p->setPosition(pos);
        p->m_position = pos;
        p->m_yVelocity = 0;
        p->setVisible(false);
    }

    void levelComplete() {
        auto f = m_fields.self();
        if (f->spectating) return;
        bool first = !f->completed;
        f->completed = true;
        PlayLayer::levelComplete();
        if (f->natal <= 0 || m_isTestMode || m_isPracticeMode || !first) return;
        if (f->battle) sendFinish();
        f->reward.rpc("geobr_natal_level_complete", matjson::makeObject({{"p_level", f->natal}, {"p_elapsed", f->elapsed}}), [](auto r) {
            if (!r.success) return;
            if (r.value.contains("state")) natal::setState(r.value["state"]);
            auto candy = jsonutil::integerOr(r.value, "candy");
            if (candy > 0) Notification::create(fmt::format("Natal: +{} Candy!", candy), NotificationIcon::Success, 3.f)->show();
        });
    }

    // The equipped accessory follows the icon with a little spring (physics):
    //  - cube, ball, robot, spider: stays upright and leans with the jump;
    //  - ship, jetpack and UFO: sits on the small icon inside and tilts with it;
    //  - wave and swing: no icon to wear it, so it hides.
    // Scarves (neck slot) wrap the icon: the knot/tail behind, the band in front.
    void followPlayer(float dt) {
        auto f = m_fields.self();
        auto p = m_player1;
        if (!p) return;
        if (!f->followerTried) {
            f->followerTried = true;
            auto id = myAccessory();
            auto art = natal::makeAccessory(id, 30.f);
            auto parent = p->getParent();
            if (art.sprite && parent && !typeinfo_cast<CCSpriteBatchNode*>(parent)) {
                art.sprite->setID("geobr-natal-follower");
                f->follower = art.sprite;
                f->offset = art.offset;
                f->baseScale = art.sprite->getScale();
                parent->addChild(art.sprite, p->getZOrder() + (art.behind ? -1 : 1));
            }
        }
        auto fol = f->follower;
        if (!fol) return;
        bool hiddenMode = p->m_isDart || p->m_isSwing;
        bool show = p->isVisible() && !p->m_isDead && !hiddenMode && fol->getParent() == p->getParent();
        fol->setVisible(show);
        if (f->band) f->band->setVisible(show);
        if (!show) return;
        auto parent = fol->getParent();
        CCPoint center = p->getPosition();
        float scale = p->m_vehicleSize > 0 ? p->m_vehicleSize : 1.f;
        float target;
        bool flipped = p->m_isUpsideDown;
        // The part that wears it: the little icon inside a ship/UFO/jetpack, or the head of the robot/spider.
        CCSprite* wearer = nullptr;
        if ((p->m_isShip || p->m_isBird) && p->m_iconSprite && p->m_iconSprite->isVisible()) wearer = p->m_iconSprite;
        else if (p->m_isRobot && p->m_robotSprite && p->m_robotSprite->m_headSprite) wearer = p->m_robotSprite->m_headSprite;
        else if (p->m_isSpider && p->m_spiderSprite && p->m_spiderSprite->m_headSprite) wearer = p->m_spiderSprite->m_headSprite;
        if (wearer && wearer->isVisible()) {
            // Its world centre and "up" give position, size and tilt.
            auto icon = wearer;
            auto cs = icon->getContentSize();
            CCPoint c = parent->convertToNodeSpace(icon->convertToWorldSpace(cs / 2));
            CCPoint up = parent->convertToNodeSpace(icon->convertToWorldSpace({cs.width / 2.f, cs.height}));
            CCPoint d = up - c;
            float len = std::max(1.f, d.getLength());
            scale = len / 15.f;
            target = std::atan2(d.x, d.y) * 180.f / 3.14159265f;
            center = c;
            flipped = false;  // the direction already says which way is up
        } else {
            target = std::clamp(static_cast<float>(p->m_yVelocity) * -1.6f, -16.f, 16.f) * (flipped ? -1.f : 1.f) + (flipped ? 180.f : 0.f);
        }
        // Spring towards the target angle (shortest way round).
        float diff = std::fmod(target - f->tilt + 540.f, 360.f) - 180.f;
        f->spin += diff * 160.f * dt;
        f->spin *= std::exp(-11.f * dt);
        f->tilt += f->spin * dt;
        if (std::abs(diff) > 120.f) { f->tilt = target; f->spin = 0; }   // mode changes snap
        float rad = f->tilt * 3.14159265f / 180.f;
        CCPoint off = f->offset * scale;
        CCPoint rotated{off.x * std::cos(rad) + off.y * std::sin(rad), -off.x * std::sin(rad) + off.y * std::cos(rad)};
        fol->setScale(f->baseScale * scale);
        fol->setRotation(f->tilt);
        fol->setPosition(center + rotated);
        if (f->band) {
            f->band->setScale(f->baseScale * scale);
            f->band->setRotation(f->tilt);
            // The band sits where the top of the scarf is (the sprite's upper part).
            float half = fol->getContentHeight() * fol->getScaleY() * .5f;
            CCPoint topOff{std::sin(rad) * half * .16f, std::cos(rad) * half * .16f};
            f->band->setPosition(fol->getPosition() + topOff);
        }
    }

    void sendProgress(bool finished) {
        auto f = m_fields.self();
        Ref<PlayLayer> keep = this;
        f->progress.rpc("geobr_natal_arena_progress", matjson::makeObject({
            {"p_percent", finished ? 100 : getCurrentPercentInt()},
            {"p_finished", finished}, {"p_ms", static_cast<int>(f->attempt * 1000.f)},
        }), [keep](auto r) {
            if (!r.success || !r.value.contains("arena") || !r.value["arena"].isObject()) return;
            natal::setBattle(r.value["arena"]);
            auto self = static_cast<GeoBRNatalPlay*>(keep.data());
            if (!self->getParent()) return;
            auto const& arena = r.value["arena"];
            if (jsonutil::stringOr(arena, "state") != "finished") return;
            // Someone beat the level: stop reporting and show who won.
            self->m_fields->battle = false;
            auto winner = jsonutil::stringOr(arena, "winner");
            self->showCenter(winner.empty() ? std::string("Fim do tempo!") : fmt::format("{} venceu!", winner), .7f);
        });
    }

    void runGuard() {
        auto mods = matjson::Value::array();
        for (auto const& id : natal::enabledMods()) mods.push(id);
        m_fields->guardCheck.rpc("geobr_natal_arena_guard", matjson::makeObject({{"p_mods", mods}}), [](auto r) {
            if (!r.success || jsonutil::boolOr(r.value, "ok", true)) return;
            std::string list;
            auto blocked = r.value["blocked"];
            if (blocked.isArray()) for (auto const& m : blocked.asArray().unwrap()) list += " " + m.asString().unwrapOr("");
            Notification::create("BattleGuard: desative" + list + " (Geode > Mods) e reinicie o jogo", NotificationIcon::Warning, 5.f)->show();
        });
    }
};
