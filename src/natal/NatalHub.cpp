#include "NatalHub.hpp"
#include "NatalService.hpp"
#include "NatalTheme.hpp"
#include "NatalSpectate.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/MainLevelLauncher.hpp"
#include "../config/GeoBRConfig.hpp"
#include "../ui/AdminPanel.hpp"
#include "../ui/LocalLevelChooser.hpp"
#include "../ui/GeoBRVisuals.hpp"
#include "../ui/UIHelpers.hpp"

#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LocalLevelManager.hpp>
#include <hjfod.gmd-api/include/GMD.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/ui/Notification.hpp>

#include <charconv>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <random>

using namespace geode::prelude;

namespace geobr {
namespace {

constexpr float W = 470.f, H = 300.f;
constexpr char const* Rarities[] = {"comum", "incomum", "raro", "epico", "lendario", "extremamente_raro"};
enum Tab { Home, Boxes, Accessories, Levels, Battle, Pass, Ranking, Owner };

CCSprite* sprite(std::string const& file, float size) {
    auto s = CCSprite::create(Mod::get()->expandSpriteName(file).data());
    if (!s) return nullptr;
    auto c = s->getContentSize();
    s->setScale(size / std::max(1.f, std::max(c.width, c.height)));
    return s;
}

CCLabelBMFont* label(CCNode* parent, std::string const& text, CCPoint pos, float scale, ccColor3B color = {255, 255, 255}, char const* font = "bigFont.fnt", float maxWidth = 0) {
    auto l = CCLabelBMFont::create(text.c_str(), font);
    l->setScale(scale);
    l->setColor(color);
    l->setPosition(pos);
    if (maxWidth > 0) l->limitLabelWidth(maxWidth, scale, .1f);
    parent->addChild(l, 3);
    return l;
}

std::vector<matjson::Value> arr(matjson::Value const& v) { return v.isArray() ? v.asArray().unwrap() : std::vector<matjson::Value>{}; }

bool contains(matjson::Value const& list, std::int64_t value) {
    for (auto const& v : arr(list)) if (v.asInt().unwrapOr(-1) == value) return true;
    return false;
}

} // namespace
std::string clockLong(double seconds);
namespace {

std::string dateBR(std::string const& iso) {
    return iso.size() >= 10 ? iso.substr(8, 2) + "/" + iso.substr(5, 2) : iso;
}

// Reward card: the accessory art (or Candy) with its name and rarity.
class RewardPopup final : public Popup {
public:
    static RewardPopup* create(matjson::Value const& reward, std::string const& title) {
        auto p = new RewardPopup;
        if (p->init(reward, title)) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    bool init(matjson::Value const& reward, std::string const& title) {
        if (!Popup::init(260.f, 200.f)) return false;
        setTitle(title.c_str(), "goldFont.fnt", .7f, 18);
        auto kind = jsonutil::stringOr(reward, "kind");
        std::string art = "natal_candy.png", line, sub;
        ccColor3B color{255, 255, 255};
        if (kind == "accessory") {
            auto a = natal::accessory(jsonutil::stringOr(reward, "id"));
            if (a) art = jsonutil::stringOr(*a, "sprite", art);
            auto rarity = jsonutil::stringOr(reward, "rarity");
            line = jsonutil::stringOr(reward, "name", "Acessorio");
            color = natal::rarityColor(rarity);
            sub = natal::rarityName(rarity);
            if (jsonutil::boolOr(reward, "duplicate", false)) sub = fmt::format("Repetido: +{} Candy", jsonutil::integerOr(reward, "refund"));
        } else if (kind == "theme" || kind == "frame") {
            line = natal::rewardText(reward);
            color = natal::rarityColor(jsonutil::stringOr(reward, "rarity"));
            sub = kind == "theme" ? "Tema de perfil" : "Moldura de perfil";
            art.clear();
            if (auto icon = natal::theme::cosmeticIcon(jsonutil::stringOr(reward, "id"), 86.f)) {
                icon->setPosition({130.f, 112.f});
                float s0 = icon->getScale();
                icon->setScale(s0 * .2f);
                icon->runAction(CCEaseBackOut::create(CCScaleTo::create(.45f, s0)));
                m_mainLayer->addChild(icon, 3);
            }
        } else {
            line = natal::rewardText(reward);
        }
        if (auto s = art.empty() ? nullptr : sprite(art, 86.f)) {
            s->setPosition({130.f, 112.f});
            s->setScale(s->getScale() * .2f);
            s->runAction(CCEaseBackOut::create(CCScaleTo::create(.45f, s->getScale() * 5.f)));
            m_mainLayer->addChild(s, 3);
        }
        label(m_mainLayer, line, {130.f, 50.f}, .5f, color, "bigFont.fnt", 230.f);
        if (!sub.empty()) label(m_mainLayer, sub, {130.f, 30.f}, .55f, {230, 230, 230}, "chatFont.fnt");
        return true;
    }
};

} // namespace

NatalHub* NatalHub::create(int tab) {
    auto p = new NatalHub;
    if (p->init(tab)) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

void NatalHub::open(int tab) {
    if (auto hub = NatalHub::create(tab)) hub->show();
}

bool NatalHub::init(int tab) {
    if (!Popup::init(W, H)) return false;
    m_tab = tab;
    visual::fitPopup(m_mainLayer, {W, H});
    visual::stylePopup(m_mainLayer, m_bgSprite, {W, H}, {120, 25, 35});
    setID("geobr-natal-hub");
    if (auto logo = sprite("natal_logo.png", 74.f)) { logo->setPosition({96.f, H - 24.f}); m_mainLayer->addChild(logo, 4); }
    if (auto candy = sprite("natal_candy.png", 26.f)) { candy->setPosition({W - 92.f, H - 24.f}); m_mainLayer->addChild(candy, 4); }
    m_candy = label(m_mainLayer, "...", {W - 52.f, H - 24.f}, .5f, {255, 230, 120}, "bigFont.fnt");
    m_status = label(m_mainLayer, "", {W / 2.f, 12.f}, .45f, {230, 230, 255}, "chatFont.fnt");
    m_content = CCNode::create();
    m_mainLayer->addChild(m_content, 2);
    schedule(schedule_selector(NatalHub::pollBattle), 2.f);
    schedule(schedule_selector(NatalHub::tickCountdown), 1.f);
    status("Carregando o Natal...");
    render();
    WeakRef<NatalHub> weak = this;
    natal::refresh([weak](bool ok, std::string const& error) {
        auto self = weak.lock();
        if (!self) return;
        self->status(ok ? "" : "Sem conexao: " + error);
        self->render();
    });
    return true;
}

void NatalHub::status(std::string const& text) {
    if (!m_status) return;
    m_status->setString(text.c_str());
    m_status->limitLabelWidth(W - 30.f, .45f, .2f);
}

CCMenu* NatalHub::menu() {
    if (!m_menu) {
        m_menu = CCMenu::create();
        m_menu->setPosition({0, 0});
        m_content->addChild(m_menu, 5);
    }
    return m_menu;
}

CCMenuItemSpriteExtra* NatalHub::button(std::string const& text, int width, char const* bg, SEL_MenuHandler handler, int tag, CCPoint pos, float scale) {
    auto b = ui::makeTextButton(text, width, bg, this, handler, scale, 26.f);
    b->setTag(tag);
    b->setPosition(pos);
    menu()->addChild(b);
    return b;
}

void NatalHub::call(std::string const& fn, matjson::Value args, std::function<void(matjson::Value const&)> ok) {
    if (m_busy) return;
    m_busy = true;
    status("...");
    WeakRef<NatalHub> weak = this;
    m_action.rpc(fn, std::move(args), [weak, ok](auto r) {
        auto self = weak.lock();
        if (!self) return;
        self->m_busy = false;
        if (!r.success) {
            if (r.error.find("BattleGuard") != std::string::npos) FLAlertLayer::create("BattleGuard", r.error, "OK")->show();
            self->status(r.error);
            return;
        }
        self->status("");
        if (r.value.isObject() && r.value.contains("state")) natal::setState(r.value["state"]);
        ok(r.value);
    });
}

void NatalHub::render() {
    m_content->removeAllChildren();
    m_menu = nullptr;
    m_wheel = nullptr;
    m_pointer = nullptr;
    m_inputA = m_inputB = nullptr;
    m_countdown = nullptr;
    auto me = natal::state().get("me");
    m_candy->setString(me ? std::to_string(jsonutil::integerOr(me.unwrap(), "candy")).c_str() : "-");
    // Tabs.
    std::vector<std::string> tabs = {"Inicio", "Loja", "Acessorios", "Evento", "Batalha", "Passe", "Ranking"};
    if (natal::isAdmin()) tabs.push_back("Dono");
    float tabW = (W - 20.f) / tabs.size();
    for (size_t i = 0; i < tabs.size(); ++i) {
        button(tabs[i], static_cast<int>(tabW - 6), static_cast<int>(i) == m_tab ? "GJ_button_01.png" : "GJ_button_04.png",
            menu_selector(NatalHub::onTab), static_cast<int>(i), {10.f + tabW * (i + .5f), H - 61.f}, .27f);
    }
    if (m_tab == Owner && !natal::isAdmin()) m_tab = Home;
    switch (m_tab) {
        case Home: renderHome(); break;
        case Boxes: renderBoxes(); break;
        case Accessories: renderAccessories(); break;
        case Levels: renderLevels(); break;
        case Battle: renderBattle(); break;
        case Pass: renderPass(); break;
        case Ranking: renderRanking(); break;
        case Owner: renderOwner(); break;
    }
}

// --------------------------------------------------------------- home
namespace {
constexpr float WheelR = 60.f;
const ccColor4F SliceColors[] = {{.82f, .12f, .17f, 1.f}, {.11f, .52f, .25f, 1.f}, {.96f, .94f, .89f, 1.f}};
const ccColor4F Gold{.98f, .80f, .30f, 1.f};
int sliceColor(int i, int n) { return n % 3 == 0 ? i % 3 : (n % 2 == 0 ? i % 2 : (i == n - 1 ? 2 : i % 2)); }
float rad(float deg) { return deg * 3.14159265f / 180.f; }
} // namespace

void NatalHub::renderHome() {
    auto const& s = natal::state();
    auto event = s.get("event");
    if (event) {
        auto e = event.unwrap();
        std::string line;
        if (jsonutil::boolOr(e, "public", false)) line = fmt::format("Evento aberto ate {}", dateBR(jsonutil::stringOr(e, "ends_at")));
        else if (jsonutil::boolOr(e, "testing", false)) line = fmt::format("MODO TESTE (so a staff ve) - hoje no evento: {}", dateBR(jsonutil::stringOr(e, "today")));
        else line = fmt::format("O evento abre em {}. Volte para pegar seus presentes!", dateBR(jsonutil::stringOr(e, "starts_at")));
        if (jsonutil::stringOr(e, "arena") == "lobby") line += "  |  BATALHA NATALINA ABERTA!";
        label(m_content, line, {W / 2.f, H - 80.f}, .45f, jsonutil::boolOr(e, "testing", false) ? ccColor3B{255, 200, 120} : ccColor3B{180, 255, 190}, "chatFont.fnt", W - 30.f);
    }
    // A real roulette: coloured slices, gold rim with bulbs, hub and a fixed pointer on top.
    CCPoint c{125.f, 120.f};
    label(m_content, "Roleta diaria", {c.x, 203.f}, .45f, {255, 220, 120}, "goldFont.fnt");
    auto rim = CCDrawNode::create();
    rim->setPosition(c);
    rim->drawDot({0, 0}, WheelR + 8.f, ccc4f(.42f, .06f, .10f, 1.f));
    rim->drawDot({0, 0}, WheelR + 4.5f, Gold);
    m_content->addChild(rim, 2);
    for (int i = 0; i < 16; ++i) {
        auto bulb = CCDrawNode::create();
        bulb->drawDot({0, 0}, 2.3f, i % 2 ? ccc4f(1.f, 1.f, .85f, 1.f) : ccc4f(1.f, .32f, .32f, 1.f));
        bulb->setPosition(c + CCPoint{std::sin(rad(i * 22.5f)), std::cos(rad(i * 22.5f))} * (WheelR + 6.2f));
        bulb->setVisible(i % 2 == 0);
        bulb->runAction(CCRepeatForever::create(CCSequence::create(CCDelayTime::create(.45f), CCToggleVisibility::create(), nullptr)));
        m_content->addChild(bulb, 3);
    }
    m_wheel = CCNode::create();
    m_wheel->setPosition(c);
    m_content->addChild(m_wheel, 3);
    auto segments = arr(s["roulette"]);
    int n = static_cast<int>(segments.size());
    bool spun = s.get("spin_today") && s["spin_today"].isObject();
    if (n > 0) {
        float step = 360.f / n;
        auto slices = CCDrawNode::create();
        for (int i = 0; i < n; ++i) {
            std::vector<CCPoint> pts{{0, 0}};
            for (int k = 0; k <= 12; ++k) {
                float a = rad(i * step + step * k / 12.f);
                pts.push_back({std::sin(a) * WheelR, std::cos(a) * WheelR});
            }
            slices->drawPolygon(pts.data(), static_cast<unsigned>(pts.size()), SliceColors[sliceColor(i, n)], 1.f, Gold);
        }
        m_wheel->addChild(slices);
        for (int i = 0; i < n; ++i) {
            float mid = (i + .5f) * step;
            CCPoint dir{std::sin(rad(mid)), std::cos(rad(mid))};
            auto kind = jsonutil::stringOr(segments[i], "kind");
            auto icon = kind == "box" ? sprite(fmt::format("natal_caixa_{}.png", jsonutil::stringOr(segments[i], "rarity")), 20.f) : sprite("natal_candy.png", 18.f);
            if (icon) { icon->setPosition(dir * (WheelR * .5f)); icon->setRotation(mid); m_wheel->addChild(icon, 2); }
            std::string text = kind == "candy" ? std::to_string(jsonutil::integerOr(segments[i], "amount")) : natal::rarityName(jsonutil::stringOr(segments[i], "rarity"));
            auto l = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
            l->setScale(kind == "candy" ? .3f : .2f);
            l->limitLabelWidth(30.f, l->getScale(), .1f);
            l->setColor(sliceColor(i, n) == 2 ? ccColor3B{190, 30, 40} : ccColor3B{255, 255, 255});
            l->setPosition(dir * (WheelR * .8f));
            l->setRotation(mid);
            m_wheel->addChild(l, 2);
        }
        // Gloss on the upper half and, after spinning, today's slice outlined in gold.
        auto gloss = CCDrawNode::create();
        std::vector<CCPoint> arc;
        for (int k = 0; k <= 24; ++k) { float a = rad(-90.f + 180.f * k / 24.f); arc.push_back({std::sin(a) * WheelR * .96f, std::cos(a) * WheelR * .96f}); }
        gloss->drawPolygon(arc.data(), static_cast<unsigned>(arc.size()), ccc4f(1.f, 1.f, 1.f, .10f), 0.f, ccc4f(0, 0, 0, 0));
        m_wheel->addChild(gloss, 1);
        if (spun) {
            int seg = static_cast<int>(jsonutil::integerOr(s["spin_today"], "segment"));
            std::vector<CCPoint> pts{{0, 0}};
            for (int k = 0; k <= 12; ++k) { float a = rad(seg * step + step * k / 12.f); pts.push_back({std::sin(a) * WheelR, std::cos(a) * WheelR}); }
            auto mark = CCDrawNode::create();
            mark->drawPolygon(pts.data(), static_cast<unsigned>(pts.size()), ccc4f(1.f, .9f, .4f, .25f), 2.5f, ccc4f(1.f, .85f, .2f, 1.f));
            mark->runAction(CCRepeatForever::create(CCSequence::create(CCFadeTo::create(.6f, 90), CCFadeTo::create(.6f, 255), nullptr)));
            m_wheel->addChild(mark, 1);
        }
        auto hub = CCDrawNode::create();
        hub->drawDot({0, 0}, 13.f, Gold);
        hub->drawDot({0, 0}, 10.f, ccc4f(.75f, .10f, .15f, 1.f));
        m_wheel->addChild(hub, 3);
        if (auto candy = sprite("natal_candy.png", 15.f)) m_wheel->addChild(candy, 4);
        if (spun) m_wheel->setRotation(-(static_cast<float>(jsonutil::integerOr(s["spin_today"], "segment")) + .5f) * step);
        else m_wheel->runAction(CCRepeatForever::create(CCRotateBy::create(24.f, 360.f)));
    }
    auto pointer = CCDrawNode::create();
    CCPoint tri[] = {{-9.f, 14.f}, {9.f, 14.f}, {0.f, -6.f}};
    pointer->drawPolygon(tri, 3, Gold, 1.5f, ccc4f(.35f, .05f, .08f, 1.f));
    pointer->setPosition({c.x, c.y + WheelR + 4.f});
    m_content->addChild(pointer, 5);
    m_pointer = pointer;
    auto spin = button(spun ? "Volte amanha" : "Girar", 110, spun ? "GJ_button_04.png" : "GJ_button_01.png", menu_selector(NatalHub::onSpin), 0, {c.x, 34.f});
    spin->setEnabled(!spun);
    // Calendar until December 25 (counts from December 1st; staff can simulate a day).
    label(m_content, "Calendario ate 25/12", {350.f, 203.f}, .45f, {255, 220, 120}, "goldFont.fnt");
    int open = event ? static_cast<int>(jsonutil::integerOr(event.unwrap(), "calendar_day")) : 0;
    for (int d = 1; d <= 25; ++d) {
        int col = (d - 1) % 5, row = (d - 1) / 5;
        bool claimed = contains(s["calendar_claimed"], d);
        bool can = !claimed && d <= open;
        auto b = button(claimed ? "ok" : std::to_string(d), 30, claimed ? "GJ_button_02.png" : can ? "GJ_button_01.png" : "GJ_button_04.png",
            menu_selector(NatalHub::onDay), d, {274.f + col * 38.f, 178.f - row * 33.f}, .38f);
        b->setEnabled(can);
    }
}

void NatalHub::onSpin(CCObject*) {
    WeakRef<NatalHub> weak = this;
    call("geobr_natal_spin", matjson::Value::object(), [weak](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        auto reward = r["reward"];
        int segment = static_cast<int>(jsonutil::integerOr(reward, "segment"));
        int count = std::max<int>(1, static_cast<int>(arr(natal::state()["roulette"]).size()));
        if (self->m_wheel) {
            // Lands the slice under the pointer after five full turns.
            float target = -(segment + .5f) * 360.f / count;
            float delta = std::fmod(target - self->m_wheel->getRotation(), 360.f);
            if (delta < 0) delta += 360.f;
            self->m_wheel->stopAllActions();
            if (self->m_pointer) {
                auto ticks = CCArray::create();
                for (int i = 0; i < 26; ++i) {
                    float t = .04f + i * i * .0042f;   // slows down with the wheel
                    ticks->addObject(CCRotateTo::create(t * .4f, -18.f));
                    ticks->addObject(CCRotateTo::create(t * .6f, 0.f));
                }
                self->m_pointer->runAction(CCSequence::create(ticks));
            }
            WeakRef<NatalHub> confettiRef = self.data();
            self->runAction(CCSequence::create(CCDelayTime::create(3.4f), CallFuncExt::create([confettiRef] {
                auto hub = confettiRef.lock();
                if (!hub || !hub->m_wheel) return;
                auto at = hub->m_wheel->getPosition();
                for (int i = 0; i < 26; ++i) {
                    auto dot = CCDrawNode::create();
                    ccColor4F cols[] = {{1.f, .25f, .3f, 1.f}, {.2f, .85f, .4f, 1.f}, {1.f, .85f, .2f, 1.f}, {.5f, .8f, 1.f, 1.f}};
                    dot->drawDot({0, 0}, 2.2f, cols[i % 4]);
                    dot->setPosition(at);
                    float a = i * 13.8f * 3.14159265f / 180.f, dist = 55.f + (i % 5) * 14.f;
                    dot->runAction(CCSequence::create(CCEaseOut::create(CCMoveBy::create(.7f, {std::sin(a) * dist, std::cos(a) * dist}), 2.f),
                        CCMoveBy::create(.6f, {0, -25.f}), CCRemoveSelf::create(), nullptr));
                    hub->m_content->addChild(dot, 8);
                }
            }), nullptr));
            self->m_wheel->runAction(CCSequence::create(
                CCEaseExponentialOut::create(CCRotateBy::create(3.4f, 360.f * 5.f + delta)),
                CCCallFunc::create(self, callfunc_selector(NatalHub::render)), nullptr));
        }
        auto popup = [reward]() { if (auto p = RewardPopup::create(reward, "Roleta")) p->show(); };
        self->runAction(CCSequence::create(CCDelayTime::create(3.5f), CallFuncExt::create(popup), nullptr));
    });
}

void NatalHub::onDay(CCObject* sender) {
    WeakRef<NatalHub> weak = this;
    int day = sender->getTag();
    call("geobr_natal_calendar_claim", matjson::makeObject({{"p_day", day}}), [weak, day](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        self->render();
        if (auto p = RewardPopup::create(r["reward"], fmt::format("Dia {}", day))) p->show();
    });
}

// --------------------------------------------------------------- shop
namespace {
CCScale9Sprite* panel(CCNode* parent, CCPoint center, CCSize size, ccColor3B color, GLubyte opacity = 255) {
    auto bg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    bg->setContentSize(size);
    bg->setColor(color);
    bg->setOpacity(opacity);
    bg->setPosition(center);
    parent->addChild(bg, 1);
    return bg;
}
ccColor3B darker(ccColor3B c, float k) { return {static_cast<GLubyte>(c.r * k), static_cast<GLubyte>(c.g * k), static_cast<GLubyte>(c.b * k)}; }
} // namespace

// Loja de Doces: today's accessories (bought with Candy) and the accessory boxes.
void NatalHub::renderBoxes() {
    if (!m_shopLoaded) {
        label(m_content, "Abrindo a Loja de Doces...", {W / 2.f, 150.f}, .6f, {220, 220, 220}, "chatFont.fnt");
        WeakRef<NatalHub> weak = this;
        m_poll.rpc("geobr_natal_shop", matjson::Value::object(), [weak](auto r) {
            auto self = weak.lock();
            if (!self) return;
            if (!r.success) { self->status(r.error); return; }
            self->m_shop = r.value;
            self->m_shopLoaded = true;
            if (self->m_tab == Boxes) self->render();
        });
        return;
    }
    label(m_content, "Loja de Doces - ofertas do dia", {170.f, 205.f}, .45f, {255, 220, 120}, "goldFont.fnt");
    m_countdownTarget = jsonutil::stringOr(m_shop, "resets_at");
    m_countdownPrefix = "Renova em ";
    m_countdown = label(m_content, m_countdownPrefix + clockLong(natal::secondsUntil(m_countdownTarget)), {390.f, 205.f}, .45f, {150, 255, 160}, "chatFont.fnt");
    auto offers = arr(m_shop["offers"]);
    for (size_t i = 0; i < std::min<size_t>(offers.size(), 4); ++i) {
        auto const& o = offers[i];
        float x = 70.f + i * 110.f, y = 146.f;
        auto rarity = jsonutil::stringOr(o, "rarity");
        auto color = natal::rarityColor(rarity);
        panel(m_content, {x, y}, {100.f, 96.f}, darker(color, .45f), 230);
        if (auto art = sprite(jsonutil::stringOr(o, "sprite"), 40.f)) { art->setPosition({x, y + 18.f}); m_content->addChild(art, 3); }
        label(m_content, jsonutil::stringOr(o, "name"), {x, y - 12.f}, .3f, {255, 255, 255}, "bigFont.fnt", 90.f);
        label(m_content, natal::rarityName(rarity), {x, y - 24.f}, .4f, color, "chatFont.fnt", 90.f);
        bool owned = jsonutil::boolOr(o, "owned", false);
        auto b = button(owned ? "Ja tem" : fmt::format("{} Candy", jsonutil::integerOr(o, "price")), 86, owned ? "GJ_button_04.png" : "GJ_button_01.png",
            menu_selector(NatalHub::onBuy), static_cast<int>(i), {x, y - 42.f}, .28f);
        b->setEnabled(!owned);
    }
    label(m_content, "Caixas surpresa", {W / 2.f, 82.f}, .42f, {255, 220, 120}, "goldFont.fnt");
    auto prices = m_shop.contains("box_prices") ? m_shop["box_prices"] : natal::state()["box_prices"];
    for (int i = 0; i < 6; ++i) {
        float x = 48.f + i * 75.f;
        std::string r = Rarities[i];
        if (auto s = sprite(fmt::format("natal_caixa_{}.png", r), 26.f)) { s->setPosition({x - 18.f, 52.f}); m_content->addChild(s, 3); }
        button(std::to_string(jsonutil::integerOr(prices, r)), 40, "GJ_button_02.png", menu_selector(NatalHub::onBox), i, {x + 14.f, 52.f}, .3f);
        label(m_content, natal::rarityName(r), {x, 32.f}, .34f, natal::rarityColor(r), "chatFont.fnt", 70.f);
    }
}

void NatalHub::onBuy(CCObject* sender) {
    auto offers = arr(m_shop["offers"]);
    if (sender->getTag() < 0 || sender->getTag() >= static_cast<int>(offers.size())) return;
    auto id = jsonutil::stringOr(offers[sender->getTag()], "id");
    WeakRef<NatalHub> weak = this;
    call("geobr_natal_buy", matjson::makeObject({{"p_accessory", id}}), [weak](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        if (r.contains("shop")) self->m_shop = r["shop"];
        self->render();
        if (auto p = RewardPopup::create(r["reward"], "Loja de Doces")) p->show();
    });
}

void NatalHub::onBox(CCObject* sender) {
    WeakRef<NatalHub> weak = this;
    std::string rarity = Rarities[std::clamp(sender->getTag(), 0, 5)];
    call("geobr_natal_open_box", matjson::makeObject({{"p_rarity", rarity}}), [weak, rarity](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        self->m_shopLoaded = false;  // "Ja tem" may change
        self->render();
        if (auto p = RewardPopup::create(r["reward"], "Caixa " + natal::rarityName(rarity))) p->show();
    });
}

// --------------------------------------------------------------- accessories
namespace {
bool owns(std::string const& id) {
    for (auto const& o : arr(natal::state()["inventory"])) if (o.asString().unwrapOr("") == id) return true;
    return false;
}
std::string sourceText(matjson::Value const& a) {
    auto source = jsonutil::stringOr(a, "source");
    if (source == "passe") return "exclusivo do Passe de Natal";
    if (source == "calendario") return "vem no Calendario";
    return "sai nas caixas e na Loja de Doces";
}
} // namespace

// Every accessory is shown in colour; the ones you don't have get a lock and
// can be previewed on your icon before you get them.
void NatalHub::renderAccessories() {
    char const* subPages[] = {"Acessorios", "Temas", "Molduras"};
    for (int i = 0; i < 3; ++i)
        button(subPages[i], 84, i == m_accPage ? "GJ_button_02.png" : "GJ_button_04.png", menu_selector(NatalHub::onAccPage), i, {180.f + i * 92.f, 207.f}, .28f);
    if (m_accPage > 0) { renderCosmetics(m_accPage == 1 ? "theme" : "frame"); return; }
    auto gm = GameManager::sharedState();
    auto player = SimplePlayer::create(gm->getPlayerFrame());
    player->setColor(gm->colorForIdx(gm->getPlayerColor()));
    player->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
    player->setScale(1.9f);
    player->setPosition({80.f, 150.f});
    m_content->addChild(player, 3);
    auto equipped = natal::equipped();
    bool previewing = !m_preview.empty() && m_preview != equipped;
    auto shown = previewing ? m_preview : equipped;
    natal::attach(player, shown);
    auto current = natal::accessory(shown);
    label(m_content, current ? jsonutil::stringOr(*current, "name") : std::string("Nenhum"), {80.f, 96.f}, .32f,
        previewing ? ccColor3B{170, 220, 255} : ccColor3B{255, 230, 140}, "bigFont.fnt", 140.f);
    if (previewing && current) {
        label(m_content, fmt::format("Previa - {}", sourceText(*current)), {80.f, 82.f}, .36f, {200, 220, 255}, "chatFont.fnt", 150.f);
        button("Voltar", 70, "GJ_button_04.png", menu_selector(NatalHub::onEquip), -2, {80.f, 58.f}, .32f);
    } else {
        button("Tirar", 70, "GJ_button_06.png", menu_selector(NatalHub::onEquip), -1, {80.f, 62.f}, .32f);
    }
    auto all = arr(natal::accessories());
    int have = 0;
    for (auto const& a : all) if (owns(jsonutil::stringOr(a, "id"))) ++have;
    label(m_content, fmt::format("Colecao: {}/{}", have, all.size()), {420.f, 207.f}, .32f, {255, 220, 120}, "goldFont.fnt");
    int perPage = 15, pages = std::max<int>(1, (static_cast<int>(all.size()) + perPage - 1) / perPage);
    m_page = std::clamp(m_page, 0, pages - 1);
    for (int i = 0; i < perPage; ++i) {
        size_t index = m_page * perPage + i;
        if (index >= all.size()) break;
        auto const& a = all[index];
        auto id = jsonutil::stringOr(a, "id");
        bool mine = owns(id);
        CCPoint at{190.f + (i % 5) * 54.f, 178.f - (i / 5) * 46.f};
        auto slot = panel(m_content, at, {46.f, 42.f}, id == shown ? ccColor3B{255, 210, 90} : darker(natal::rarityColor(jsonutil::stringOr(a, "rarity")), .5f), mine ? 230 : 140);
        (void)slot;
        auto art = sprite(jsonutil::stringOr(a, "sprite"), 32.f);
        if (!art) continue;
        if (!mine) art->setOpacity(150);
        auto item = CCMenuItemSpriteExtra::create(art, this, menu_selector(NatalHub::onEquip));
        item->setTag(static_cast<int>(index));
        item->setPosition(at);
        menu()->addChild(item);
        if (!mine) {
            if (auto lock = CCSprite::createWithSpriteFrameName("GJ_lock_001.png")) {
                lock->setScale(.32f);
                lock->setPosition({at.x + 15.f, at.y - 13.f});
                m_content->addChild(lock, 6);
            }
        } else if (id == equipped) {
            label(m_content, "USANDO", {at.x, at.y - 17.f}, .22f, {150, 255, 160}, "bigFont.fnt");
        }
    }
    button("<", 30, "GJ_button_04.png", menu_selector(NatalHub::onPage), -1, {200.f, 34.f}, .4f);
    label(m_content, fmt::format("{}/{}", m_page + 1, pages), {300.f, 34.f}, .4f);
    button(">", 30, "GJ_button_04.png", menu_selector(NatalHub::onPage), 1, {400.f, 34.f}, .4f);
}

// Temas / Molduras: a little profile card on the left shows how it looks
// (theme behind, your icon with the accessory, frame around); the grid on the
// right lists every one: owned ones equip, priced ones can be bought.
void NatalHub::renderCosmetics(std::string const& kind) {
    auto const& s = natal::state();
    auto me = s.get("me") ? s["me"] : matjson::Value::object();
    auto equipped = jsonutil::stringOr(me, kind.c_str());
    auto shown = !m_preview.empty() ? m_preview : equipped;
    CCSize card{150.f, 100.f};
    auto holder = CCNode::create();
    holder->setContentSize(card);
    holder->setPosition({8.f, 98.f});
    m_content->addChild(holder, 3);
    auto themeId = kind == "theme" ? shown : jsonutil::stringOr(me, "theme");
    auto frameId = kind == "frame" ? shown : jsonutil::stringOr(me, "frame");
    holder->addChild(visual::theme(card, themeId.empty() ? "cosmos" : themeId), 0);
    auto gm = GameManager::sharedState();
    auto player = SimplePlayer::create(gm->getPlayerFrame());
    player->setColor(gm->colorForIdx(gm->getPlayerColor()));
    player->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
    player->setScale(1.2f);
    player->setPosition({card.width / 2.f, card.height / 2.f - 6.f});
    holder->addChild(player, 5);
    natal::attach(player, natal::equipped());
    natal::theme::profileFrame(holder, frameId);
    auto current = natal::cosmetic(shown);
    label(m_content, current ? jsonutil::stringOr(*current, "name") : std::string(kind == "theme" ? "Sem tema" : "Sem moldura"), {83.f, 84.f}, .3f,
        !m_preview.empty() && m_preview != equipped ? ccColor3B{170, 220, 255} : ccColor3B{255, 230, 140}, "bigFont.fnt", 150.f);
    if (!m_preview.empty() && m_preview != equipped) button("Voltar", 70, "GJ_button_04.png", menu_selector(NatalHub::onCosmetic), -2, {83.f, 58.f}, .3f);
    else if (!equipped.empty()) button("Tirar", 70, "GJ_button_06.png", menu_selector(NatalHub::onCosmetic), -1, {83.f, 58.f}, .3f);
    label(m_content, kind == "theme" ? "Aparece no seu perfil para todos." : "Aparece em volta do seu perfil.", {83.f, 36.f}, .38f, {210, 210, 230}, "chatFont.fnt", 150.f);
    auto all = arr(natal::cosmetics());
    int i = 0;
    for (size_t index = 0; index < all.size(); ++index) {
        auto const& c = all[index];
        if (jsonutil::stringOr(c, "kind") != kind) continue;
        auto id = jsonutil::stringOr(c, "id");
        bool owned = natal::ownsCosmetic(id);
        CCPoint at{208.f + (i % 4) * 70.f, 168.f - (i / 4) * 78.f};
        panel(m_content, at, {62.f, 58.f}, id == shown ? ccColor3B{255, 210, 90} : darker(natal::rarityColor(jsonutil::stringOr(c, "rarity")), .5f), owned ? 230 : 150);
        if (auto icon = natal::theme::cosmeticIcon(id, 42.f)) {
            auto item = CCMenuItemSpriteExtra::create(icon, this, menu_selector(NatalHub::onCosmetic));
            item->setTag(static_cast<int>(index));
            item->setPosition({at.x, at.y + 4.f});
            menu()->addChild(item);
        }
        label(m_content, jsonutil::stringOr(c, "name"), {at.x, at.y - 24.f}, .18f, {255, 255, 255}, "bigFont.fnt", 60.f);
        auto price = jsonutil::integerOr(c, "price");
        if (owned) label(m_content, id == equipped ? std::string("USANDO") : std::string("Toque p/ usar"), {at.x, at.y - 37.f}, .2f,
            id == equipped ? ccColor3B{150, 255, 160} : ccColor3B{220, 220, 220}, "bigFont.fnt", 62.f);
        else if (price > 0) button(fmt::format("{} Candy", price), 62, "GJ_button_01.png", menu_selector(NatalHub::onCosmetic), 1000 + static_cast<int>(index), {at.x, at.y - 39.f}, .22f);
        else label(m_content, jsonutil::stringOr(c, "source") == "passe" ? std::string("Passe") : std::string("Evento"), {at.x, at.y - 37.f}, .2f, {255, 200, 120}, "bigFont.fnt");
        ++i;
    }
}

void NatalHub::onAccPage(CCObject* sender) {
    m_accPage = std::clamp(sender->getTag(), 0, 2);
    m_preview.clear();
    status("");
    render();
}

void NatalHub::onCosmetic(CCObject* sender) {
    int tag = sender->getTag();
    std::string kind = m_accPage == 1 ? "theme" : "frame";
    if (tag == -2) { m_preview.clear(); render(); return; }
    auto all = arr(natal::cosmetics());
    WeakRef<NatalHub> weak = this;
    if (tag >= 1000) {
        int index = tag - 1000;
        if (index >= static_cast<int>(all.size())) return;
        call("geobr_natal_buy_cosmetic", matjson::makeObject({{"p_id", jsonutil::stringOr(all[index], "id")}}), [weak](matjson::Value const& r) {
            auto self = weak.lock();
            if (!self) return;
            self->m_preview.clear();
            self->render();
            if (auto p = RewardPopup::create(r["reward"], "Loja de Doces")) p->show();
        });
        return;
    }
    matjson::Value id = nullptr;
    if (tag >= 0) {
        if (tag >= static_cast<int>(all.size())) return;
        auto wanted = jsonutil::stringOr(all[tag], "id");
        if (!natal::ownsCosmetic(wanted)) {
            m_preview = wanted;
            auto price = jsonutil::integerOr(all[tag], "price");
            status(price > 0 ? fmt::format("{}: compre por {} Candy", jsonutil::stringOr(all[tag], "name"), price)
                             : fmt::format("{}: vem no Passe de Natal", jsonutil::stringOr(all[tag], "name")));
            render();
            return;
        }
        id = wanted;
    }
    m_preview.clear();
    call("geobr_natal_equip_cosmetic", matjson::makeObject({{"p_kind", kind}, {"p_id", id}}), [weak](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        natal::setState(r);
        self->render();
    });
}

void NatalHub::onEquip(CCObject* sender) {
    int tag = sender->getTag();
    if (tag == -2) { m_preview.clear(); render(); return; }
    matjson::Value id = nullptr;
    if (tag >= 0) {
        auto all = arr(natal::accessories());
        if (tag >= static_cast<int>(all.size())) return;
        auto const& a = all[tag];
        auto wanted = jsonutil::stringOr(a, "id");
        if (!owns(wanted)) {
            m_preview = wanted;
            status(fmt::format("{} ({}) - {}", jsonutil::stringOr(a, "name"), natal::rarityName(jsonutil::stringOr(a, "rarity")), sourceText(a)));
            render();
            return;
        }
        id = wanted;
    }
    m_preview.clear();
    WeakRef<NatalHub> weak = this;
    call("geobr_natal_equip", matjson::makeObject({{"p_accessory", id}}), [weak](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        natal::setState(r);
        self->render();
    });
}

// --------------------------------------------------------------- levels
std::string clockLong(double seconds) {
    long s = std::max(0L, static_cast<long>(std::ceil(seconds)));
    if (s >= 86400) return fmt::format("{}d {:02}h {:02}m", s / 86400, (s % 86400) / 3600, (s % 3600) / 60);
    return fmt::format("{:02}:{:02}:{:02}", s / 3600, (s % 3600) / 60, s % 60);
}

// Evento: the next Batalha Natalina (every hour by itself, or when the owner
// marks one) with a countdown, how it works and the levels it uses.
void NatalHub::renderLevels() {
    auto const& a = natal::battle();
    m_countdown = nullptr;
    if (!m_arenaLoaded) { label(m_content, "Carregando...", {W / 2.f, 150.f}, .6f, {220, 220, 220}, "chatFont.fnt"); pollBattle(0); return; }
    auto card = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    card->setContentSize({250.f, 172.f});
    card->setColor({0, 0, 0});
    card->setOpacity(80);
    card->setPosition({140.f, 132.f});
    m_content->addChild(card, 1);
    label(m_content, "Proxima Batalha Natalina", {140.f, 205.f}, .5f, {255, 220, 120}, "goldFont.fnt");
    auto state = jsonutil::stringOr(a, "state");
    if (state == "lobby") {
        bool open = natal::secondsUntil(jsonutil::stringOr(a, "opens_at")) <= 0;
        label(m_content, open ? "Entrada aberta! Comeca em" : "Comeca em", {140.f, 178.f}, .5f, {255, 255, 255}, "chatFont.fnt");
        m_countdownTarget = jsonutil::stringOr(a, "lobby_until");
        m_countdownPrefix = "";
        m_countdown = label(m_content, clockLong(natal::secondsUntil(m_countdownTarget)), {140.f, 152.f}, .75f, {150, 255, 160}, "bigFont.fnt");
        label(m_content, fmt::format("({})", dateBR(m_countdownTarget)), {140.f, 128.f}, .45f, {200, 220, 255}, "chatFont.fnt");
        label(m_content, jsonutil::boolOr(a, "auto", false) ? "Tem batalha toda hora! Aviso com 5 min e 1 min."
                                                             : "Batalha marcada pelo Dono. Aviso com 1 h, 5 min e 1 min.",
            {140.f, 106.f}, .4f, {220, 220, 220}, "chatFont.fnt", 236.f);
    } else if (state == "running") {
        label(m_content, "Batalha rolando agora!", {140.f, 165.f}, .6f, {150, 255, 160}, "chatFont.fnt");
        auto level = a.get("level") ? jsonutil::stringOr(a["level"], "name") : std::string();
        if (!level.empty()) label(m_content, "Fase: " + level, {140.f, 140.f}, .5f, {255, 255, 255}, "chatFont.fnt", 236.f);
    } else {
        label(m_content, "Nenhuma batalha marcada agora.", {140.f, 160.f}, .55f, {220, 220, 220}, "chatFont.fnt", 236.f);
        label(m_content, "Fique de olho: voce recebe um aviso antes.", {140.f, 138.f}, .45f, {200, 220, 255}, "chatFont.fnt", 236.f);
    }
    button("Ir para a Batalha", 150, "GJ_button_01.png", menu_selector(NatalHub::onTab), Battle, {140.f, 72.f}, .32f);
    // Right: how it works and the levels.
    label(m_content, "Como funciona", {372.f, 205.f}, .45f, {255, 220, 120}, "goldFont.fnt");
    char const* rules[] = {"1. Entre na espera (1 minuto)", "2. Vote na fase", "3. Todos comecam juntos",
                           "4. Quem passar primeiro ganha!", "Top 3 + Candy pra quem jogar"};
    for (int i = 0; i < 5; ++i) {
        auto l = label(m_content, rules[i], {290.f, 184.f - i * 15.f}, .42f, i == 3 ? ccColor3B{255, 220, 120} : ccColor3B{255, 255, 255}, "chatFont.fnt", 180.f);
        l->setAnchorPoint({0.f, .5f});
    }
    label(m_content, "Fases da batalha", {372.f, 100.f}, .4f, {200, 220, 255}, "goldFont.fnt");
    auto maps = arr(a["maps"]);
    if (maps.empty()) label(m_content, "Nenhuma fase publicada ainda.", {372.f, 82.f}, .4f, {200, 200, 200}, "chatFont.fnt");
    for (size_t i = 0; i < maps.size() && i < 4; ++i) {
        auto l = label(m_content, "- " + jsonutil::stringOr(maps[i], "name"), {290.f, 84.f - i * 13.f}, .4f, {255, 255, 255}, "chatFont.fnt", 180.f);
        l->setAnchorPoint({0.f, .5f});
    }
}

void NatalHub::tickCountdown(float) {
    if (!m_countdown || m_countdownTarget.empty()) return;
    double left = natal::secondsUntil(m_countdownTarget);
    m_countdown->setString((m_countdownPrefix + clockLong(left)).c_str());
    // The level changed: reload the state so the new one shows up.
    if (left <= 0 && !m_busy) { m_countdownTarget.clear(); m_shopLoaded = false; WeakRef<NatalHub> weak = this; natal::refresh([weak](bool, std::string const&) { if (auto self = weak.lock()) self->render(); }); }
}

void NatalHub::onPlay(CCObject* sender) {
    // Tag = -1 - level id.
    if (auto row = natal::level(-1 - static_cast<std::int64_t>(sender->getTag()))) playLevel(*row);
}

void NatalHub::playLevel(matjson::Value const& row) {
    auto level = natal::mainLevelOf(row);
    if (auto local = MainLevelLauncher::cached(level); local) {
        CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(.3f, PlayLayer::scene(local.level, false, false)));
        return;
    }
    status("Baixando a fase...");
    WeakRef<NatalHub> weak = this;
    m_download.spawn("GeoBR - fase de Natal", MainLevelLauncher::download(level), [weak, level](auto response) {
        auto self = weak.lock();
        auto imported = MainLevelLauncher::import(level, response);
        if (!imported) { if (self) self->status(imported.error); return; }
        CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(.45f, PlayLayer::scene(imported.level, false, false)));
    });
}

// --------------------------------------------------------------- battle
// Batalha Natalina (classic only): the owner opens it, everyone presses Entrar
// during the lobby (3 minutes by default) and votes one of the owner's event
// levels, then all play it at the same time; whoever beats it first wins (no
// one in time: furthest %). The server moves it along; this screen asks every 2s.
namespace {
std::string clock(double seconds) {
    int s = std::max(0, static_cast<int>(std::ceil(seconds)));
    return fmt::format("{}:{:02}", s / 60, s % 60);
}
std::string score(matjson::Value const& p) {
    if (jsonutil::boolOr(p, "disqualified", false)) return "DQ";
    if (jsonutil::boolOr(p, "finished", false)) return "PASSOU!";
    return fmt::format("{}%", jsonutil::integerOr(p, "percent"));
}
} // namespace

void NatalHub::renderBattle() {
    auto const& a = natal::battle();
    label(m_content, "Batalha Natalina", {W / 2.f, H - 84.f}, .55f, {255, 220, 120}, "goldFont.fnt");
    if (!m_arenaLoaded) { label(m_content, "Carregando...", {W / 2.f, 150.f}, .6f, {220, 220, 220}, "chatFont.fnt"); pollBattle(0); return; }
    auto state = jsonutil::stringOr(a, "state");
    if (state.empty()) {
        label(m_content, "A batalha ainda nao comecou.", {W / 2.f, 175.f}, .65f, {255, 255, 255}, "chatFont.fnt");
        label(m_content, "Quando o Dono abrir, o botao ENTRAR aparece aqui.", {W / 2.f, 152.f}, .55f, {200, 220, 255}, "chatFont.fnt");
        label(m_content, "Todo mundo entra, vota na fase e joga junto. Quem passar a fase primeiro ganha!", {W / 2.f, 130.f}, .5f, {200, 200, 200}, "chatFont.fnt", W - 40.f);
        label(m_content, "Voce recebe um aviso quando ela for marcada (1 hora, 5 minutos e 1 minuto antes).", {W / 2.f, 110.f}, .42f, {200, 220, 255}, "chatFont.fnt", W - 40.f);
        if (natal::isAdmin()) button("Abrir pelo Menu do Dono", 170, "GJ_button_02.png", menu_selector(NatalHub::onTab), Owner, {W / 2.f, 80.f}, .3f);
        return;
    }
    bool joined = jsonutil::boolOr(a, "joined", false);
    auto players = arr(a["players"]);
    // Players (left).
    int humans = 0, bots = 0;
    for (auto const& p : players) (jsonutil::boolOr(p, "bot", false) ? bots : humans)++;
    label(m_content, fmt::format("Jogadores: {}{}", humans, bots ? fmt::format(" + {} bots", bots) : std::string()), {110.f, 200.f}, .5f, {255, 220, 120}, "chatFont.fnt");
    for (size_t i = 0; i < std::min<size_t>(players.size(), 7); ++i) {
        auto const& p = players[i];
        auto place = jsonutil::integerOr(p, "place");
        std::string text = fmt::format("{}{}{}", place > 0 ? fmt::format("{}. ", place) : std::string(), jsonutil::stringOr(p, "name"),
            jsonutil::boolOr(p, "bot", false) ? " (bot)" : "");
        if (state != "lobby") text += "  " + score(p);
        if (jsonutil::integerOr(p, "prize") > 0) text += fmt::format("  +{}", jsonutil::integerOr(p, "prize"));
        ccColor3B color = jsonutil::boolOr(p, "me", false) ? ccColor3B{255, 230, 120} : jsonutil::boolOr(p, "bot", false) ? ccColor3B{170, 200, 255} : ccColor3B{255, 255, 255};
        auto l = label(m_content, text, {20.f, 180.f - i * 18.f}, .5f, color, "chatFont.fnt", 190.f);
        l->setAnchorPoint({0.f, .5f});
    }
    if (players.size() > 7) label(m_content, fmt::format("+{} jogadores", players.size() - 7), {110.f, 52.f}, .45f, {200, 200, 200}, "chatFont.fnt");
    // Right side: what to do now.
    float cx = 345.f;
    if (state == "lobby" && natal::secondsUntil(jsonutil::stringOr(a, "opens_at")) > 0) {
        // Scheduled: the entry opens 1 minute before the start.
        double left = natal::secondsUntil(jsonutil::stringOr(a, "lobby_until"));
        label(m_content, "Batalha agendada!", {cx, 200.f}, .6f, {255, 220, 120}, "goldFont.fnt");
        label(m_content, fmt::format("Comeca em {}", clockLong(left)), {cx, 170.f}, .6f, {150, 255, 160}, "bigFont.fnt", 230.f);
        label(m_content, fmt::format("({})", dateBR(jsonutil::stringOr(a, "lobby_until"))), {cx, 148.f}, .45f, {200, 220, 255}, "chatFont.fnt", 230.f);
        label(m_content, "A entrada abre 1 minuto antes. Voce recebe aviso com 1 hora, 5 minutos e 1 minuto.", {cx, 118.f}, .42f, {220, 220, 220}, "chatFont.fnt", 230.f);
        label(m_content, "Quem passar a fase primeiro ganha!", {cx, 90.f}, .48f, {255, 220, 120}, "chatFont.fnt", 230.f);
        return;
    }
    if (state == "lobby") {
        double left = natal::secondsUntil(jsonutil::stringOr(a, "lobby_until"));
        label(m_content, fmt::format("Entrada aberta!  Comeca em {}", clock(left)), {cx, 200.f}, .55f, {150, 255, 160}, "chatFont.fnt", 230.f);
        if (!joined) {
            button("ENTRAR", 150, "GJ_button_01.png", menu_selector(NatalHub::onEnterArena), 0, {cx, 150.f}, .5f);
            label(m_content, "Entre agora para jogar e votar na fase.", {cx, 115.f}, .45f, {220, 220, 220}, "chatFont.fnt", 230.f);
            return;
        }
        label(m_content, "Voce esta dentro! Vote na fase:", {cx, 180.f}, .48f, {255, 255, 255}, "chatFont.fnt", 230.f);
        auto maps = arr(a["maps"]);
        auto mine = jsonutil::integerOr(a, "my_vote");
        int perPage = 4, pages = std::max<int>(1, (static_cast<int>(maps.size()) + perPage - 1) / perPage);
        m_page = std::clamp(m_page, 0, pages - 1);
        for (int i = 0; i < perPage; ++i) {
            size_t index = m_page * perPage + i;
            if (index >= maps.size()) break;
            auto const& m = maps[index];
            bool chosen = jsonutil::integerOr(m, "id") == mine;
            auto text = fmt::format("{} ({})", jsonutil::stringOr(m, "name"), jsonutil::integerOr(m, "votes"));
            button(text, 210, chosen ? "GJ_button_01.png" : "GJ_button_04.png", menu_selector(NatalHub::onVote), static_cast<int>(index), {cx, 155.f - i * 26.f}, .26f);
        }
        if (pages > 1) {
            button("<", 26, "GJ_button_04.png", menu_selector(NatalHub::onPage), -1, {cx - 60.f, 52.f}, .36f);
            button(">", 26, "GJ_button_04.png", menu_selector(NatalHub::onPage), 1, {cx + 60.f, 52.f}, .36f);
        }
        button("Sair", 50, "GJ_button_06.png", menu_selector(NatalHub::onArenaLeave), 0, {cx, 52.f}, .3f);
        return;
    }
    auto levelName = a.get("level") ? jsonutil::stringOr(a["level"], "name") : std::string("?");
    label(m_content, fmt::format("Fase: {}", levelName), {cx, 200.f}, .55f, {255, 255, 255}, "chatFont.fnt", 230.f);
    label(m_content, "Quem passar a fase primeiro ganha", {cx, 182.f}, .45f, {200, 220, 255}, "chatFont.fnt", 230.f);
    if (state == "running" && jsonutil::boolOr(a, "loading", false)) {
        label(m_content, "Abrindo a fase para todos...", {cx, 158.f}, .55f, {255, 220, 120}, "chatFont.fnt", 230.f);
        label(m_content, fmt::format("Prontos: {}/{}  (comeca quando todos carregarem)", jsonutil::integerOr(a, "ready"), jsonutil::integerOr(a, "humans")),
            {cx, 136.f}, .45f, {200, 220, 255}, "chatFont.fnt", 230.f);
        if (joined) button("ABRIR A FASE", 150, "GJ_button_01.png", menu_selector(NatalHub::onPlayBattle), 0, {cx, 100.f}, .45f);
        else label(m_content, "Voce nao entrou nessa. Espere a proxima!", {cx, 100.f}, .5f, {220, 220, 220}, "chatFont.fnt", 230.f);
        return;
    }
    if (state == "running") {
        double start = natal::secondsUntil(jsonutil::stringOr(a, "starts_at")), end = natal::secondsUntil(jsonutil::stringOr(a, "ends_at"));
        label(m_content, start > 0 ? fmt::format("Comeca em {}", clock(start)) : fmt::format("Termina em {}", clock(end)), {cx, 155.f}, .7f, {255, 220, 120}, "bigFont.fnt");
        bool passed = jsonutil::boolOr(a, "my_finished", false);
        if (joined && !passed) {
            auto play = button(start > 0 ? "Prepare-se..." : "JOGAR", 150, "GJ_button_01.png", menu_selector(NatalHub::onPlayBattle), 0, {cx, 112.f}, .45f);
            play->setEnabled(start <= 0);
        } else if (!joined) {
            label(m_content, "Voce nao entrou nessa. Espere a proxima!", {cx, 118.f}, .45f, {220, 220, 220}, "chatFont.fnt", 230.f);
        } else {
            label(m_content, "Voce passou! Seu resultado ja conta.", {cx, 118.f}, .45f, {150, 255, 160}, "chatFont.fnt", 230.f);
        }
        auto winner = jsonutil::stringOr(a, "winner");
        if (!winner.empty()) label(m_content, "1o lugar: " + winner, {cx, 136.f}, .45f, {255, 220, 120}, "chatFont.fnt", 230.f);
        button("Espectar a tela", 120, "GJ_button_02.png", menu_selector(NatalHub::onSpectate), 0, {cx - 62.f, 78.f}, .3f);
        button("Placar", 90, "GJ_button_04.png", menu_selector(NatalHub::onScoreboard), 0, {cx + 70.f, 78.f}, .3f);
        return;
    }
    auto winner = jsonutil::stringOr(a, "winner");
    label(m_content, state == "cancelled" ? std::string("Batalha cancelada (ninguem entrou).")
        : !winner.empty() ? fmt::format("Fim! {} passou primeiro e venceu!", winner) : std::string("Fim! Ninguem passou: venceu quem chegou mais longe."),
        {cx, 150.f}, .5f, {255, 220, 120}, "chatFont.fnt", 230.f);
    label(m_content, "Fique de olho na proxima batalha.", {cx, 128.f}, .45f, {220, 220, 220}, "chatFont.fnt", 230.f);
}

void NatalHub::pollBattle(float) {
    if (m_tab != Battle && m_tab != Owner && m_tab != Levels) return;
    if (m_pollBusy) return;
    m_pollBusy = true;
    WeakRef<NatalHub> weak = this;
    m_poll.rpc("geobr_natal_arena_state", matjson::Value::object(), [weak](auto r) {
        auto self = weak.lock();
        if (!self) return;
        self->m_pollBusy = false;
        if (!r.success) { if (self->m_tab == Battle) self->status(r.error); return; }
        auto next = r.value.contains("arena") && r.value["arena"].isObject() ? r.value["arena"] : matjson::Value::object();
        bool changed = !self->m_arenaLoaded || next.dump() != natal::battle().dump();
        self->m_arenaLoaded = true;
        natal::setBattle(next);
        if (self->m_tab == Owner) self->m_config["arena"] = next;
        auto state = jsonutil::stringOr(next, "state");
        // The clocks tick, so a live battle redraws every poll.
        if ((changed || (self->m_tab == Battle && (state == "lobby" || state == "running"))) && !self->m_busy && self->m_tab != Levels) self->render();
        else if (changed && self->m_tab == Levels && !self->m_busy) self->render();
    });
}

void NatalHub::onEnterArena(CCObject*) {
    std::string list;
    auto guard = natal::battle()["guard"];
    for (auto const& mine : natal::enabledMods()) {
        bool bad = mine == "geobr.speedhack";
        if (guard.isArray()) for (auto const& g : guard.asArray().unwrap()) bad = bad || utils::string::toLower(g.asString().unwrapOr("")) == utils::string::toLower(mine);
        if (bad) list += (list.empty() ? "" : ", ") + mine;
    }
    if (!list.empty()) {
        FLAlertLayer::create("BattleGuard", "Para entrar na batalha, desative <cr>" + list + "</c> em <cg>Geode > Mods</c> e reinicie o jogo.", "OK")->show();
        return;
    }
    auto mods = matjson::Value::array();
    for (auto const& id : natal::enabledMods()) mods.push(id);
    WeakRef<NatalHub> weak = this;
    call("geobr_natal_arena_join", matjson::makeObject({{"p_mods", mods}}), [weak](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        natal::setBattle(r.contains("arena") && r["arena"].isObject() ? r["arena"] : matjson::Value::object());
        self->status("Voce entrou! Vote no mapa.");
        self->render();
    });
}

void NatalHub::onVote(CCObject* sender) {
    auto maps = arr(natal::battle()["maps"]);
    if (sender->getTag() < 0 || sender->getTag() >= static_cast<int>(maps.size())) return;
    WeakRef<NatalHub> weak = this;
    call("geobr_natal_arena_vote", matjson::makeObject({{"p_level", jsonutil::integerOr(maps[sender->getTag()], "id")}}), [weak](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        natal::setBattle(r.contains("arena") && r["arena"].isObject() ? r["arena"] : matjson::Value::object());
        self->render();
    });
}

void NatalHub::onArenaLeave(CCObject*) {
    WeakRef<NatalHub> weak = this;
    call("geobr_natal_arena_leave", matjson::Value::object(), [weak](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        natal::setBattle(r.contains("arena") && r["arena"].isObject() ? r["arena"] : matjson::Value::object());
        self->render();
    });
}

void NatalHub::onSpectate(CCObject*) {
    status("Abrindo a fase para espectar...");
    natal::openBattleLevel(true);
}

void NatalHub::onScoreboard(CCObject*) {
    if (auto p = natal::SpectatePopup::create()) p->show();
}

void NatalHub::onPlayBattle(CCObject*) {
    status("Abrindo a fase da batalha...");
    natal::openBattleLevel();
}

// --------------------------------------------------------------- pass
// One Natal pass (no paid track): reward cards on a numbered track. The Candy
// collected during the event moves you along it; tap a gold card to claim.
void NatalHub::renderPass() {
    auto const& s = natal::state();
    // The pass follows the Candy you have now (spending lowers it too).
    auto lifetime = s.get("me") ? jsonutil::integerOr(s["me"], "candy") : 0;
    auto tiers = arr(s["pass"]);
    int count = static_cast<int>(tiers.size());
    int next = 0;
    while (next < count && lifetime >= jsonutil::integerOr(tiers[next], "need")) ++next;
    label(m_content, "Passe de Natal", {84.f, 205.f}, .5f, {255, 220, 120}, "goldFont.fnt");
    label(m_content, next < count ? fmt::format("Seus Candy: {}   |   nivel {} em {} Candy (gastar tambem conta)", lifetime, next + 1, jsonutil::integerOr(tiers[next], "need") - lifetime)
                                  : fmt::format("Seus Candy: {}   |   passe completo!", lifetime),
        {305.f, 205.f}, .45f, {255, 255, 255}, "chatFont.fnt", 300.f);
    if (count == 0) return;
    constexpr int perPage = 5;
    int pages = (count + perPage - 1) / perPage;
    if (m_page < 0) m_page = std::min(next, count - 1) / perPage;  // opens on the page you are in
    m_page = std::clamp(m_page, 0, pages - 1);
    constexpr float x0 = 62.f, dx = 87.f, cardY = 145.f, trackY = 80.f;
    int first = m_page * perPage, shown = std::min(perPage, count - first);
    // Track with the gold fill up to where your Candy is.
    float pos = -0.5f;
    for (int j = 0; j < shown; ++j) {
        auto need = jsonutil::integerOr(tiers[first + j], "need");
        auto prev = first + j > 0 ? jsonutil::integerOr(tiers[first + j - 1], "need") : 0;
        if (lifetime >= need) { pos = static_cast<float>(j); continue; }
        if (lifetime > prev) pos = j - 1 + static_cast<float>(lifetime - prev) / std::max<int64_t>(1, need - prev);
        break;
    }
    float left = x0 - 30.f, right = x0 + dx * (shown - 1) + 30.f;
    auto track = CCLayerColor::create({45, 18, 30, 255}, right - left, 8.f);
    track->setPosition({left, trackY - 4.f});
    m_content->addChild(track, 2);
    float fillTo = std::clamp(x0 + pos * dx, left, right);
    if (fillTo > left) {
        auto fill = CCLayerColor::create({250, 195, 45, 255}, fillTo - left, 8.f);
        fill->setPosition({left, trackY - 4.f});
        m_content->addChild(fill, 3);
    }
    for (int j = 0; j < shown; ++j) {
        int tier = first + j + 1;
        auto const& t = tiers[first + j];
        auto need = jsonutil::integerOr(t, "need");
        bool claimed = contains(s["pass_claimed"], tier), unlocked = lifetime >= need;
        float x = x0 + j * dx;
        // Card (tap to claim).
        ccColor3B color = claimed ? ccColor3B{45, 125, 70} : unlocked ? ccColor3B{235, 175, 40} : ccColor3B{100, 36, 52};
        auto card = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
        card->setContentSize({78.f, 92.f});
        card->setColor(color);
        auto reward = t["reward"];
        auto kind = jsonutil::stringOr(reward, "kind");
        std::string art = "natal_candy.png", text;
        if (kind == "box") { art = fmt::format("natal_caixa_{}.png", jsonutil::stringOr(reward, "rarity")); text = "Caixa " + natal::rarityName(jsonutil::stringOr(reward, "rarity")); }
        else if (kind == "theme" || kind == "frame") {
            art.clear();
            text = natal::rewardText(reward);
            if (auto icon = natal::theme::cosmeticIcon(jsonutil::stringOr(reward, "id"), 42.f)) { icon->setPosition({39.f, 54.f}); card->addChild(icon, 2); }
        }
        else if (kind == "accessory") {
            if (auto a = natal::accessory(jsonutil::stringOr(reward, "id"))) { art = jsonutil::stringOr(*a, "sprite", art); text = jsonutil::stringOr(*a, "name"); }
        } else text = fmt::format("{} Candy", jsonutil::integerOr(reward, "amount"));
        if (auto icon = art.empty() ? nullptr : sprite(art, kind == "candy" ? 34.f : 42.f)) { icon->setPosition({39.f, 54.f}); card->addChild(icon, 2); }
        auto name = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        name->setScale(.26f);
        name->limitLabelWidth(70.f, .26f, .1f);
        name->setPosition({39.f, 18.f});
        card->addChild(name, 2);
        if (kind == "accessory" || kind == "theme" || kind == "frame") {
            auto tag = CCLabelBMFont::create("EXCLUSIVO", "bigFont.fnt");
            tag->setScale(.2f);
            tag->setColor({255, 230, 120});
            tag->setPosition({39.f, 84.f});
            card->addChild(tag, 3);
        }
        if (claimed) {
            if (auto ok = CCSprite::createWithSpriteFrameName("GJ_completesIcon_001.png")) { ok->setScale(.55f); ok->setPosition({66.f, 80.f}); card->addChild(ok, 4); }
        } else if (!unlocked) {
            if (auto lock = CCSprite::createWithSpriteFrameName("GJ_lock_001.png")) { lock->setScale(.4f); lock->setPosition({66.f, 12.f}); card->addChild(lock, 4); }
        }
        auto item = CCMenuItemSpriteExtra::create(card, this, menu_selector(NatalHub::onClaimTier));
        item->setTag(tier);
        item->setPosition({x, cardY});
        menu()->addChild(item);
        if (unlocked && !claimed) {
            card->runAction(CCRepeatForever::create(CCSequence::create(CCScaleTo::create(.5f, 1.05f), CCScaleTo::create(.5f, 1.f), nullptr)));
            label(m_content, "PEGAR!", {x, cardY - 54.f}, .3f, {255, 240, 150}, "bigFont.fnt");
        }
        // Numbered node on the track.
        auto node = CCDrawNode::create();
        node->drawDot({0, 0}, 11.f, unlocked ? ccc4f(.98f, .77f, .18f, 1.f) : ccc4f(.35f, .35f, .4f, 1.f));
        node->drawDot({0, 0}, 9.f, unlocked ? ccc4f(1.f, .86f, .35f, 1.f) : ccc4f(.5f, .5f, .55f, 1.f));
        node->setPosition({x, trackY});
        m_content->addChild(node, 4);
        label(m_content, std::to_string(tier), {x, trackY}, .38f, unlocked ? ccColor3B{90, 40, 10} : ccColor3B{255, 255, 255}, "bigFont.fnt");
        label(m_content, fmt::format("{}", need), {x, trackY - 18.f}, .38f, {230, 220, 255}, "chatFont.fnt");
    }
    button("<", 30, "GJ_button_04.png", menu_selector(NatalHub::onPage), -1, {170.f, 36.f}, .4f);
    label(m_content, fmt::format("{}/{}", m_page + 1, pages), {W / 2.f, 36.f}, .4f);
    button(">", 30, "GJ_button_04.png", menu_selector(NatalHub::onPage), 1, {300.f, 36.f}, .4f);
}

void NatalHub::onClaimTier(CCObject* sender) {
    int tier = sender->getTag();
    auto const& s = natal::state();
    auto tiers = arr(s["pass"]);
    if (tier < 1 || tier > static_cast<int>(tiers.size())) return;
    auto lifetime = s.get("me") ? jsonutil::integerOr(s["me"], "candy") : 0;
    auto need = jsonutil::integerOr(tiers[tier - 1], "need");
    if (contains(s["pass_claimed"], tier)) { status(fmt::format("Nivel {} ja foi pego.", tier)); return; }
    if (lifetime < need) { status(fmt::format("Nivel {}: voce precisa ter {} Candy (faltam {}).", tier, need, need - lifetime)); return; }
    WeakRef<NatalHub> weak = this;
    call("geobr_natal_pass_claim", matjson::makeObject({{"p_tier", tier}}), [weak, tier](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        self->render();
        if (auto p = RewardPopup::create(r["reward"], fmt::format("Passe - Nivel {}", tier))) p->show();
    });
}

// --------------------------------------------------------------- ranking
// Candy ranking: Candy collected in the event (spending does not lower it).
// Top 5 on the left (the top 1 at the end is the event winner), the rest on the right.
void NatalHub::renderRanking() {
    label(m_content, "Ranking de Candy", {W / 2.f, 205.f}, .55f, {255, 220, 120}, "goldFont.fnt");
    if (!m_rankLoaded) {
        label(m_content, "Carregando...", {W / 2.f, 150.f}, .6f, {220, 220, 220}, "chatFont.fnt");
        m_rankLoaded = true;
        WeakRef<NatalHub> weak = this;
        m_rankReq.rpc("geobr_natal_ranking", matjson::makeObject({{"p_offset", m_rankPage * 8}}), [weak](auto r) {
            auto self = weak.lock();
            if (!self) return;
            if (!r.success) { self->status(r.error); return; }
            self->m_ranking = r.value;
            if (self->m_tab == Ranking) self->render();
        });
        return;
    }
    auto top = arr(m_ranking["top"]);
    bool ended = jsonutil::boolOr(m_ranking, "ended", false);
    label(m_content, ended ? (top.empty() ? std::string("Evento encerrado.") : fmt::format("Evento encerrado! Vencedor: {}", jsonutil::stringOr(top[0], "name")))
                           : std::string("Quem tiver mais Candy no fim do evento vence! (Candy juntado, gastar nao diminui)"),
        {W / 2.f, 187.f}, .42f, ended ? ccColor3B{150, 255, 160} : ccColor3B{220, 220, 220}, "chatFont.fnt", W - 30.f);
    // Top 5.
    auto panel = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    panel->setContentSize({222.f, 136.f});
    panel->setColor({0, 0, 0});
    panel->setOpacity(80);
    panel->setPosition({126.f, 112.f});
    m_content->addChild(panel, 1);
    label(m_content, "TOP 5", {126.f, 172.f}, .45f, {255, 220, 120}, "goldFont.fnt");
    static constexpr ccColor3B medal[] = {{255, 200, 40}, {200, 210, 225}, {215, 130, 60}, {150, 110, 220}, {150, 110, 220}};
    if (top.empty()) label(m_content, "Ninguem juntou Candy ainda.", {126.f, 112.f}, .45f, {200, 200, 200}, "chatFont.fnt");
    for (size_t i = 0; i < top.size() && i < 5; ++i) {
        auto const& t = top[i];
        float y = 152.f - i * 22.f;
        auto dot = CCDrawNode::create();
        auto c = medal[i];
        dot->drawDot({0, 0}, 8.f, ccc4f(c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f));
        dot->setPosition({30.f, y});
        m_content->addChild(dot, 3);
        label(m_content, std::to_string(i + 1), {30.f, y}, .3f, {40, 20, 10}, "bigFont.fnt");
        bool me = jsonutil::boolOr(t, "me", false);
        auto name = label(m_content, jsonutil::stringOr(t, "name") + (i == 0 && ended ? "  - VENCEDOR" : ""), {44.f, y}, .32f,
            me ? ccColor3B{255, 230, 120} : ccColor3B{255, 255, 255}, "bigFont.fnt", 140.f);
        name->setAnchorPoint({0.f, .5f});
        auto candy = label(m_content, fmt::format("{}", jsonutil::integerOr(t, "candy")), {226.f, y}, .4f, {255, 220, 120}, "chatFont.fnt");
        candy->setAnchorPoint({1.f, .5f});
    }
    // The rest, 8 per page.
    auto rest = arr(m_ranking["rows"]);
    label(m_content, "Demais posicoes", {350.f, 172.f}, .42f, {200, 220, 255}, "goldFont.fnt");
    if (rest.empty()) label(m_content, m_rankPage > 0 ? "Fim da lista." : "So o top 5 por enquanto.", {350.f, 120.f}, .45f, {200, 200, 200}, "chatFont.fnt");
    for (size_t i = 0; i < rest.size() && i < 8; ++i) {
        auto const& t = rest[i];
        bool me = jsonutil::boolOr(t, "me", false);
        auto row = label(m_content, fmt::format("{}. {}", jsonutil::integerOr(t, "pos"), jsonutil::stringOr(t, "name")), {262.f, 154.f - i * 13.f}, .42f,
            me ? ccColor3B{255, 230, 120} : ccColor3B{255, 255, 255}, "chatFont.fnt", 140.f);
        row->setAnchorPoint({0.f, .5f});
        auto candy = label(m_content, fmt::format("{}", jsonutil::integerOr(t, "candy")), {448.f, 154.f - i * 13.f}, .42f, {255, 220, 120}, "chatFont.fnt");
        candy->setAnchorPoint({1.f, .5f});
    }
    int total = static_cast<int>(jsonutil::integerOr(m_ranking, "total"));
    int pages = std::max(1, (std::max(0, total - 5) + 7) / 8);
    if (pages > 1) {
        button("<", 26, "GJ_button_04.png", menu_selector(NatalHub::onRankPage), -1, {310.f, 42.f}, .34f);
        label(m_content, fmt::format("{}/{}", m_rankPage + 1, pages), {350.f, 42.f}, .38f);
        button(">", 26, "GJ_button_04.png", menu_selector(NatalHub::onRankPage), 1, {390.f, 42.f}, .34f);
    }
    auto mine = m_ranking.get("me");
    label(m_content, mine && mine.unwrap().isObject()
            ? fmt::format("Voce: {}o lugar ({} Candy)", jsonutil::integerOr(mine.unwrap(), "pos"), jsonutil::integerOr(mine.unwrap(), "candy"))
            : std::string("Voce ainda nao esta no ranking"),
        {126.f, 32.f}, .45f, {255, 230, 120}, "chatFont.fnt", 220.f);
}

void NatalHub::onRankPage(CCObject* sender) {
    int total = static_cast<int>(jsonutil::integerOr(m_ranking, "total"));
    int pages = std::max(1, (std::max(0, total - 5) + 7) / 8);
    m_rankPage = std::clamp(m_rankPage + sender->getTag(), 0, pages - 1);
    m_rankLoaded = false;
    render();
}

// --------------------------------------------------------------- owner
// Menu do Dono in four pages (Evento, Batalha, Jogadores, Fases) so nothing is
// squeezed. Fases: the owner publishes one of his own levels (Criar > Meus
// levels) as an event level / battle map. Every action is checked and logged by the server.
namespace {
enum OwnerAction { ToggleEvent = 1, ToggleTest, DayDown, DayUp, RotDown, RotUp, BotsDown, BotsUp, ArenaOpen, ArenaStart, ArenaClose,
                   PlayerView, PlayerCandy, PlayerBan, PlayerUnban, OpenLevels, UnlockAll, BattleDown, BattleUp, LevelPrev, LevelNext,
                   ScheduleDown, ScheduleUp, StaffAdd, StaffRemove, LevelToggle = 200 };
constexpr int ScheduleMinutes[] = {0, 5, 15, 30, 60, 120, 180};
constexpr int BattleMinutes[] = {2, 3, 5, 8, 10, 15, 20, 30};
constexpr int LevelsPerPage = 3;
} // namespace

void NatalHub::renderOwner() {
    auto const& cfg = m_config.contains("config") ? m_config["config"] : m_config;
    if (!cfg.contains("enabled")) {
        label(m_content, "Carregando o Menu do Dono...", {W / 2.f, 150.f}, .6f, {220, 220, 220}, "chatFont.fnt");
        WeakRef<NatalHub> weak = this;
        m_action.rpc("geobr_natal_admin", matjson::makeObject({{"p_action", "overview"}, {"p_data", matjson::Value::object()}}), [weak](auto r) {
            auto self = weak.lock();
            if (!self) return;
            if (!r.success) { self->status(r.error); return; }
            self->m_config = r.value;
            if (self->m_tab == Owner) self->render();
        });
        return;
    }
    char const* pages[] = {"Evento", "Batalha", "Jogadores", "Fases"};
    for (int i = 0; i < 4; ++i)
        button(pages[i], 100, i == m_ownerPage ? "GJ_button_02.png" : "GJ_button_04.png", menu_selector(NatalHub::onOwnerPage), i, {72.f + i * 109.f, 207.f}, .3f);
    auto row = [&](std::string const& text, int down, int up, float y) {
        button("-", 26, "GJ_button_04.png", menu_selector(NatalHub::onOwner), down, {W / 2.f - 130.f, y}, .42f);
        label(m_content, text, {W / 2.f, y}, .55f, {255, 255, 255}, "chatFont.fnt", 220.f);
        button("+", 26, "GJ_button_04.png", menu_selector(NatalHub::onOwner), up, {W / 2.f + 130.f, y}, .42f);
    };
    if (m_ownerPage == 0) {
        bool enabled = jsonutil::boolOr(cfg, "enabled", false), test = jsonutil::boolOr(cfg, "test_mode", false);
        int day = static_cast<int>(jsonutil::integerOr(cfg, "test_day"));
        button(enabled ? "Evento para todos: LIGADO" : "Evento para todos: DESLIGADO", 250, enabled ? "GJ_button_01.png" : "GJ_button_06.png", menu_selector(NatalHub::onOwner), ToggleEvent, {W / 2.f, 175.f}, .32f);
        label(m_content, fmt::format("Para todos abre de {} ate {}", dateBR(jsonutil::stringOr(cfg, "starts_at")), dateBR(jsonutil::stringOr(cfg, "ends_at"))), {W / 2.f, 156.f}, .42f, {200, 220, 255}, "chatFont.fnt");
        button(test ? "Modo teste da staff: SIM" : "Modo teste da staff: NAO", 250, test ? "GJ_button_02.png" : "GJ_button_04.png", menu_selector(NatalHub::onOwner), ToggleTest, {W / 2.f, 132.f}, .32f);
        row(day > 0 ? fmt::format("Dia de teste: {}/12", day) : std::string("Dia de teste: hoje (real)"), DayDown, DayUp, 104.f);
        int every = static_cast<int>(jsonutil::integerOr(cfg, "auto_battle_minutes", 60));
        row(every <= 0 ? std::string("Batalha automatica: desligada") : every % 60 == 0 ? fmt::format("Batalha automatica: a cada {} h", every / 60)
                                                                                       : fmt::format("Batalha automatica: a cada {} min", every),
            RotDown, RotUp, 76.f);
        label(m_content, fmt::format("{} jogadores  |  {} Candy no total  |  {}/{} fases publicadas", jsonutil::integerOr(m_config, "players"), jsonutil::integerOr(m_config, "candy_total"),
            jsonutil::integerOr(m_config, "levels_published"), jsonutil::integerOr(m_config, "levels_total")), {W / 2.f, 46.f}, .42f, {200, 220, 255}, "chatFont.fnt", W - 40.f);
    } else if (m_ownerPage == 1) {
        auto const& arena = m_config.contains("arena") && m_config["arena"].isObject() ? m_config["arena"] : natal::battle();
        auto state = jsonutil::stringOr(arena, "state");
        int botCount = state == "lobby" ? static_cast<int>(jsonutil::integerOr(arena, "bots")) : m_bots;
        (void)botCount;
        label(m_content, "Sem bots: so quem ficar sozinho na batalha ganha um bot.", {W / 2.f, 168.f}, .42f, {200, 200, 200}, "chatFont.fnt", W - 40.f);
        if (state == "lobby" && natal::secondsUntil(jsonutil::stringOr(arena, "opens_at")) > 0) {
            label(m_content, fmt::format("Agendada: comeca em {}", clockLong(natal::secondsUntil(jsonutil::stringOr(arena, "lobby_until")))), {W / 2.f, 122.f}, .55f, {150, 255, 160}, "chatFont.fnt");
            button("Comecar agora", 130, "GJ_button_01.png", menu_selector(NatalHub::onOwner), ArenaStart, {W / 2.f - 80.f, 86.f}, .32f);
            button("Cancelar", 110, "GJ_button_06.png", menu_selector(NatalHub::onOwner), ArenaClose, {W / 2.f + 80.f, 86.f}, .32f);
        } else if (state == "lobby") {
            label(m_content, fmt::format("Esperando jogadores: {}s", std::max(0, static_cast<int>(natal::secondsUntil(jsonutil::stringOr(arena, "lobby_until"))))), {W / 2.f, 122.f}, .55f, {150, 255, 160}, "chatFont.fnt");
            button("Comecar agora", 130, "GJ_button_01.png", menu_selector(NatalHub::onOwner), ArenaStart, {W / 2.f - 80.f, 86.f}, .32f);
            button("Cancelar", 110, "GJ_button_06.png", menu_selector(NatalHub::onOwner), ArenaClose, {W / 2.f + 80.f, 86.f}, .32f);
        } else if (state == "running") {
            label(m_content, fmt::format("Rolando agora: {}", arena.get("level") ? jsonutil::stringOr(arena["level"], "name") : std::string()), {W / 2.f, 122.f}, .55f, {255, 220, 120}, "chatFont.fnt", 300.f);
            button("Encerrar agora", 150, "GJ_button_06.png", menu_selector(NatalHub::onOwner), ArenaClose, {W / 2.f, 86.f}, .34f);
        } else {
            auto secs = cfg.contains("battle_seconds") ? jsonutil::integerOr(cfg["battle_seconds"], "classic", 300) : 300;
            row(fmt::format("Tempo para passar a fase: {} min", std::max<int64_t>(1, secs / 60)), BattleDown, BattleUp, 128.f);
            int when = ScheduleMinutes[std::clamp(m_schedule, 0, 6)];
            row(when == 0 ? std::string("Comeca: agora (1 min de espera)") : when >= 60 ? fmt::format("Comeca: daqui {} h", when / 60) : fmt::format("Comeca: daqui {} min", when),
                ScheduleDown, ScheduleUp, 100.f);
            button(when == 0 ? "Abrir Batalha Natalina" : "Agendar Batalha Natalina", 200, "GJ_button_01.png", menu_selector(NatalHub::onOwner), ArenaOpen, {W / 2.f, 66.f}, .36f);
            label(m_content, "Todos recebem aviso: 1 hora, 5 minutos e 1 minuto antes.", {W / 2.f, 44.f}, .4f, {200, 220, 255}, "chatFont.fnt", W - 40.f);
        }
    } else if (m_ownerPage == 2) {
        label(m_content, "Usuario do GDPS (deixe vazio para voce):", {W / 2.f, 184.f}, .42f, {220, 220, 220}, "chatFont.fnt");
        m_inputA = TextInput::create(250.f, "Usuario", "chatFont.fnt");
        m_inputA->setPosition({W / 2.f, 164.f});
        m_content->addChild(m_inputA, 4);
        m_inputB = TextInput::create(250.f, "Candy (ex: 500) ou motivo", "chatFont.fnt");
        m_inputB->setPosition({W / 2.f, 132.f});
        m_content->addChild(m_inputB, 4);
        char const* names[] = {"Ver", "Dar Candy", "Suspender", "Liberar"};
        int tags[] = {PlayerView, PlayerCandy, PlayerBan, PlayerUnban};
        for (int i = 0; i < 4; ++i)
            button(names[i], 96, i == 2 ? "GJ_button_06.png" : "GJ_button_04.png", menu_selector(NatalHub::onOwner), tags[i], {72.f + i * 109.f, 96.f}, .3f);
        if (jsonutil::boolOr(m_config, "is_owner", false)) {
            button("Liberar acessorios", 150, "GJ_button_02.png", menu_selector(NatalHub::onOwner), UnlockAll, {90.f, 62.f}, .28f);
            button("Dar painel", 100, "GJ_button_01.png", menu_selector(NatalHub::onOwner), StaffAdd, {235.f, 62.f}, .28f);
            button("Tirar painel", 100, "GJ_button_06.png", menu_selector(NatalHub::onOwner), StaffRemove, {355.f, 62.f}, .28f);
            std::string names;
            for (auto const& n : arr(m_config["staff"])) names += (names.empty() ? "" : ", ") + n.asString().unwrapOr("");
            label(m_content, "Painel do Dono: voce" + (names.empty() ? std::string(" (so voce)") : ", " + names), {W / 2.f, 38.f}, .4f, {200, 220, 255}, "chatFont.fnt", W - 40.f);
        } else {
            button("Liberar TODOS os acessorios", 240, "GJ_button_02.png", menu_selector(NatalHub::onOwner), UnlockAll, {W / 2.f, 58.f}, .32f);
        }
    } else {
        label(m_content, "Fases da Batalha Natalina", {W / 2.f, 188.f}, .45f, {255, 220, 120}, "chatFont.fnt");
        button("Publicar uma fase minha", 200, "GJ_button_01.png", menu_selector(NatalHub::onPublishMine), 0, {W / 2.f, 164.f}, .32f);
        auto levels = arr(m_config["levels"]);
        if (levels.empty()) {
            label(m_content, "Nenhuma fase ainda. Escolha um level seu em Criar > Meus levels.", {W / 2.f, 110.f}, .45f, {220, 220, 220}, "chatFont.fnt", W - 40.f);
            return;
        }
        int pages = std::max<int>(1, (static_cast<int>(levels.size()) + LevelsPerPage - 1) / LevelsPerPage);
        m_levelPage = std::clamp(m_levelPage, 0, pages - 1);
        for (int i = 0; i < LevelsPerPage; ++i) {
            size_t index = m_levelPage * LevelsPerPage + i;
            if (index >= levels.size()) break;
            auto const& l = levels[index];
            float y = 132.f - i * 27.f;
            bool on = jsonutil::boolOr(l, "published", false);
            auto name = label(m_content, jsonutil::stringOr(l, "name"), {40.f, y + 4.f}, .36f, on ? ccColor3B{255, 255, 255} : ccColor3B{160, 160, 160}, "bigFont.fnt", 230.f);
            name->setAnchorPoint({0.f, .5f});
            auto info = label(m_content, jsonutil::boolOr(l, "platformer", false) ? std::string("plataforma: nao entra na Batalha") : fmt::format("por {}", jsonutil::stringOr(l, "creator")),
                {40.f, y - 8.f}, .38f, {200, 220, 255}, "chatFont.fnt", 230.f);
            info->setAnchorPoint({0.f, .5f});
            button(on ? "Publicada" : "Escondida", 110, on ? "GJ_button_01.png" : "GJ_button_06.png", menu_selector(NatalHub::onOwner), LevelToggle + static_cast<int>(index), {370.f, y}, .28f);
        }
        if (pages > 1) {
            button("<", 26, "GJ_button_04.png", menu_selector(NatalHub::onOwner), LevelPrev, {W / 2.f - 50.f, 48.f}, .36f);
            label(m_content, fmt::format("{}/{}", m_levelPage + 1, pages), {W / 2.f, 48.f}, .4f);
            button(">", 26, "GJ_button_04.png", menu_selector(NatalHub::onOwner), LevelNext, {W / 2.f + 50.f, 48.f}, .36f);
        }
        button("Detalhes (Painel ADM)", 130, "GJ_button_04.png", menu_selector(NatalHub::onOwner), OpenLevels, {W - 75.f, 48.f}, .24f);
    }
}

void NatalHub::onOwnerPage(CCObject* sender) {
    m_ownerPage = std::clamp(sender->getTag(), 0, 3);
    status("");
    render();
}

void NatalHub::onOwner(CCObject* sender) {
    int action = sender->getTag();
    auto const& cfg = m_config.contains("config") ? m_config["config"] : m_config;
    std::string player = m_inputA ? std::string(m_inputA->getString()) : "";
    std::string value = m_inputB ? std::string(m_inputB->getString()) : "";
    auto data = matjson::Value::object();
    std::string name = "config";
    auto const& arena = m_config.contains("arena") && m_config["arena"].isObject() ? m_config["arena"] : natal::battle();
    bool lobby = jsonutil::stringOr(arena, "state") == "lobby";
    if (action >= LevelToggle) {
        auto levels = arr(m_config["levels"]);
        size_t index = static_cast<size_t>(action - LevelToggle);
        if (index >= levels.size()) return;
        name = "level_publish";
        data["id"] = jsonutil::integerOr(levels[index], "id");
        data["published"] = !jsonutil::boolOr(levels[index], "published", false);
        WeakRef<NatalHub> weak = this;
        call("geobr_natal_admin", matjson::makeObject({{"p_action", name}, {"p_data", data}}), [weak](matjson::Value const& r) {
            auto self = weak.lock();
            if (!self) return;
            self->m_config = r;
            natal::refresh();
            self->status("Salvo.");
            self->render();
        });
        return;
    }
    switch (action) {
        case ToggleEvent: data["enabled"] = !jsonutil::boolOr(cfg, "enabled", false); break;
        case ToggleTest: data["test_mode"] = !jsonutil::boolOr(cfg, "test_mode", false); break;
        case DayDown: case DayUp: {
            int day = static_cast<int>(jsonutil::integerOr(cfg, "test_day")) + (action == DayUp ? 1 : -1);
            data["test_day"] = (day + 26) % 26;
            break;
        }
        case RotDown: case RotUp: {
            static constexpr int steps[] = {0, 30, 60, 120, 180};
            int current = static_cast<int>(jsonutil::integerOr(cfg, "auto_battle_minutes", 60)), index = 2;
            for (int i = 0; i < 5; ++i) if (steps[i] == current) index = i;
            int next = steps[std::clamp(index + (action == RotUp ? 1 : -1), 0, 4)];
            WeakRef<NatalHub> weak = this;
            call("geobr_natal_set_auto_battle", matjson::makeObject({{"p_minutes", next}}), [weak](matjson::Value const& r) {
                auto self = weak.lock();
                if (!self) return;
                self->m_config = r;
                self->status("Salvo.");
                self->render();
            });
            return;
        }
        case BotsDown: case BotsUp: {
            int current = lobby ? static_cast<int>(jsonutil::integerOr(arena, "bots")) : m_bots;
            int next = std::clamp(current + (action == BotsUp ? 1 : -1), 0, 8);
            if (!lobby) { m_bots = next; render(); return; }
            name = "arena_bots"; data["bots"] = next;
            break;
        }
        case BattleDown: case BattleUp: {
            auto secs = cfg.contains("battle_seconds") ? jsonutil::integerOr(cfg["battle_seconds"], "classic", 300) : 300;
            int index = 2;
            for (int i = 0; i < 8; ++i) if (BattleMinutes[i] * 60 == secs) index = i;
            data["battle_seconds"] = matjson::makeObject({{"classic", BattleMinutes[std::clamp(index + (action == BattleUp ? 1 : -1), 0, 7)] * 60}});
            break;
        }
        case LevelPrev: case LevelNext: m_levelPage += action == LevelNext ? 1 : -1; render(); return;
        case ScheduleDown: case ScheduleUp: m_schedule = std::clamp(m_schedule + (action == ScheduleUp ? 1 : -1), 0, 6); render(); return;
        case ArenaOpen: name = "arena_open"; data["minutes"] = ScheduleMinutes[std::clamp(m_schedule, 0, 6)]; break;
        case StaffAdd: case StaffRemove: name = action == StaffAdd ? "staff_add" : "staff_remove"; data["player"] = player; break;
        case ArenaStart: name = "arena_start"; break;
        case ArenaClose: name = "arena_close"; break;
        case PlayerView: name = "player"; data["player"] = player; break;
        case PlayerCandy: {
            name = "grant"; data["player"] = player;
            int64_t amount = 0;
            auto parsed = std::from_chars(value.data(), value.data() + value.size(), amount);
            if (parsed.ec != std::errc{} || amount == 0) { status("Digite a quantidade de Candy no segundo campo (pode ser negativa)."); return; }
            data["amount"] = amount;
            break;
        }
        case PlayerBan: name = "ban"; data["player"] = player; data["banned"] = true; data["reason"] = value.empty() ? "Regras do evento" : value; break;
        case PlayerUnban: name = "ban"; data["player"] = player; data["banned"] = false; break;
        case UnlockAll: {
            WeakRef<NatalHub> weak = this;
            call("geobr_natal_admin_unlock", matjson::makeObject({{"p_player", player}}), [weak, player](matjson::Value const& r) {
                auto self = weak.lock();
                if (!self) return;
                if (r.contains("state") && r["state"].isObject()) natal::setState(r["state"]);
                self->status(fmt::format("{} acessorio(s) liberado(s) para {}.", jsonutil::integerOr(r, "added"), player.empty() ? std::string("voce") : player));
            });
            return;
        }
        case OpenLevels:
            // Event levels use the GeoBR admin panel (tab Natal): Meus levels / .gmd / Da pasta.
            if (auto login = AdminLogin::create()) login->show();
            return;
        default: return;
    }
    if (((action >= PlayerView && action <= PlayerUnban) || action == StaffAdd || action == StaffRemove) && player.empty()) { status("Digite o usuario do GDPS no primeiro campo."); return; }
    WeakRef<NatalHub> weak = this;
    call("geobr_natal_admin", matjson::makeObject({{"p_action", name}, {"p_data", data}}), [weak, name](matjson::Value const& r) {
        auto self = weak.lock();
        if (!self) return;
        if (name == "config") { self->m_config = r; natal::refresh(); self->status("Salvo."); }
        else if (name == "staff_add" || name == "staff_remove") { self->m_config = r; self->status(name == "staff_add" ? "Painel liberado para ele." : "Painel retirado."); }
        else if (name.rfind("arena", 0) == 0) {
            self->m_config["arena"] = r;
            natal::setBattle(r);
            self->status(name != "arena_open" ? std::string("Feito.")
                : natal::secondsUntil(jsonutil::stringOr(r, "opens_at")) > 0 ? std::string("Batalha agendada! Todos serao avisados.")
                : std::string("Batalha aberta! Voce ja esta dentro; os outros tem 1 minuto para entrar."));
        }
        else if (name == "player") self->status(fmt::format("Candy {} (total {}), {} acessorios{}", jsonutil::integerOr(r, "candy"), jsonutil::integerOr(r, "lifetime"),
            jsonutil::integerOr(r, "accessories"), jsonutil::boolOr(r, "banned", false) ? ", SUSPENSO: " + jsonutil::stringOr(r, "reason") : ""));
        else if (name == "grant") self->status(fmt::format("Feito. Candy agora: {}", jsonutil::integerOr(r, "candy")));
        else self->status("Feito.");
        self->render();
    });
}

// One of the owner's levels becomes an event level: exported to .gmd, uploaded
// to natal-levels/ and published (classic levels are also battle maps).
void NatalHub::onPublishMine(CCObject*) {
    if (m_busy) return;
    WeakRef<NatalHub> weak = this;
    if (auto chooser = LocalLevelChooser::create([weak](GJGameLevel* level) {
        if (auto self = weak.lock(); self && level) self->publishLevel(level);
    })) chooser->show();
}

void NatalHub::publishLevel(GJGameLevel* level) {
    if (m_busy || !level) return;
    auto temp = Mod::get()->getSaveDir() / "natal-level-export.gmd";
    auto exported = gmd::exportLevelAsGmd(level, temp, gmd::GmdFileType::Gmd);
    if (exported.isErr()) { status(fmt::format("Falha ao exportar: {}", exported.unwrapErr())); return; }
    auto bytes = file::readBinary(temp);
    std::error_code removeError;
    std::filesystem::remove(temp, removeError);
    if (bytes.isErr() || bytes.unwrap().empty()) { status("Falha ao ler a fase exportada."); return; }
    if (bytes.unwrap().size() > GeoBRConfig::MaxGmdBytes) { status("A fase passa de 12 MB."); return; }
    std::string name = level->m_levelName;
    if (name.empty()) name = "Fase de Natal";
    if (name.size() > 64) name.resize(64);
    std::string creator = GameManager::sharedState()->m_playerName;
    bool platformer = level->m_levelLength == 5;
    auto path = fmt::format("natal-levels/{}-{}.gmd", std::time(nullptr), std::random_device{}());
    m_busy = true;
    status("Enviando a fase...");
    WeakRef<NatalHub> weak = this;
    int length = std::clamp(level->m_levelLength, 0, 4);  // GD length (Tiny..XL): sets the minimum time to beat it (anti-cheat)
    m_upload.call("POST", "/storage/v1/object/geobr-main-levels/" + path, matjson::Value::object(), [weak, path, name, creator, platformer, length](auto upload) {
        auto self = weak.lock();
        if (!self) return;
        self->m_busy = false;
        if (!upload.success) { self->status(upload.error); return; }
        auto data = matjson::makeObject({{"name", name}, {"creator", creator}, {"gmd_path", path}, {"platformer", platformer}, {"length", length}});
        self->call("geobr_natal_admin", matjson::makeObject({{"p_action", "level_add"}, {"p_data", data}}), [weak, name, platformer](matjson::Value const& r) {
            auto self = weak.lock();
            if (!self) return;
            self->m_config = r;
            self->m_levelPage = 0;
            natal::refresh();
            self->status(platformer ? fmt::format("{} publicada, mas fase plataforma nao entra na Batalha (so classica).", name)
                                    : fmt::format("{} publicada na Batalha Natalina!", name));
            self->render();
        });
    }, true, std::move(bytes).unwrap());
}

// --------------------------------------------------------------- nav
void NatalHub::onTab(CCObject* sender) {
    m_tab = sender->getTag();
    if (m_tab == Ranking) m_rankLoaded = false;
    m_page = m_tab == Pass ? -1 : 0;  // the pass opens on the page you are in
    m_accPage = 0;
    m_preview.clear();
    status("");
    render();
}

void NatalHub::onPage(CCObject* sender) {
    m_page += sender->getTag();
    render();
}

void NatalHub::showReward(matjson::Value const& reward, std::string const& title) {
    if (auto p = RewardPopup::create(reward, title)) p->show();
}

} // namespace geobr
