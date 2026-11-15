#include "GdpsRatePopup.hpp"
#include "UIHelpers.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>

using namespace geode::prelude;

namespace geobr {
namespace {

constexpr char const* TierNames[] = {"Rated", "Featured", "Epic", "Legendary", "Mythic"};
constexpr char const* DemonNames[] = {"Easy", "Medium", "Hard", "Insane", "Extreme"};

// GD difficulty frame for a star count (and Demon difficulty 1..5 at 10 stars).
int difficultyFor(int stars, int demon) {
    switch (stars) {
        case 1: return -1;  // Auto
        case 2: return 1;
        case 3: return 2;
        case 4: case 5: return 3;
        case 6: case 7: return 4;
        case 8: case 9: return 5;
        case 10: { static constexpr int frames[] = {7, 8, 6, 9, 10}; return frames[std::clamp(demon, 1, 5) - 1]; }
        default: return 0;  // N/A
    }
}

CCMenuItemSpriteExtra* textButton(char const* text, char const* bg, float width, CCObject* target, SEL_MenuHandler handler, int tag, float scale = .8f) {
    auto sprite = ButtonSprite::create(text, static_cast<int>(width), true, "bigFont.fnt", bg, 30, .6f);
    sprite->setScale(scale);
    auto button = CCMenuItemSpriteExtra::create(sprite, target, handler);
    button->setTag(tag);
    return button;
}

} // namespace

GdpsRatePopup* GdpsRatePopup::create(int levelID, std::string name, int stars, int tier, std::function<void()> done) {
    auto p = new GdpsRatePopup;
    if (p->init(levelID, std::move(name), stars, tier, std::move(done))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool GdpsRatePopup::init(int levelID, std::string name, int stars, int tier, std::function<void()> done) {
    if (!Popup::init(360.f, 250.f)) return false;
    m_level = levelID;
    m_name = std::move(name);
    m_stars = std::clamp(stars, 0, 10);
    m_tier = std::clamp(tier, 0, 4);
    m_done = std::move(done);
    setTitle("Rate Stars", "goldFont.fnt", .9f, 20);
    setID("geobr-gdps-rate");
    auto sub = CCLabelBMFont::create(fmt::format("{}  (ID {})", m_name, m_level).c_str(), "chatFont.fnt");
    sub->limitLabelWidth(320.f, .6f, .2f);
    sub->setPosition({180.f, 210.f});
    m_mainLayer->addChild(sub, 2);
    m_status = CCLabelBMFont::create("Toque no rosto: Featured, Epic, Legendary, Mythic", "chatFont.fnt");
    m_status->setScale(.45f);
    m_status->setPosition({180.f, 52.f});
    m_mainLayer->addChild(m_status, 2);
    render();
    return true;
}

void GdpsRatePopup::render() {
    if (m_menu) m_menu->removeFromParent();
    m_menu = CCMenu::create();
    m_menu->setPosition({0, 0});
    m_mainLayer->addChild(m_menu, 3);

    // Stars 1..10 in two rows, like GD's.
    for (int i = 1; i <= 10; ++i) {
        auto button = textButton(std::to_string(i).c_str(), i == m_stars ? "GJ_button_01.png" : "GJ_button_04.png", 18.f, this, menu_selector(GdpsRatePopup::onStar), i, .75f);
        button->setPosition({52.f + ((i - 1) % 5) * 42.f, i <= 5 ? 168.f : 128.f});
        m_menu->addChild(button);
    }

    // Difficulty face: tap to cycle the tier (only once the level has stars).
    m_face = GJDifficultySprite::create(difficultyFor(m_stars, m_demon), GJDifficultyName::Short);
    m_face->updateFeatureState(static_cast<GJFeatureState>(m_tier));
    auto face = CCMenuItemSpriteExtra::create(m_face, this, menu_selector(GdpsRatePopup::onFace));
    face->setPosition({298.f, 150.f});
    face->setID("geobr-gdps-rate-face");
    m_menu->addChild(face);
    m_tierLabel = CCLabelBMFont::create(TierNames[m_tier], "goldFont.fnt");
    m_tierLabel->setScale(.5f);
    m_tierLabel->setPosition({298.f, 112.f});
    m_menu->addChild(m_tierLabel);

    // Demon difficulty at 10 stars.
    if (m_stars == 10) {
        for (int d = 1; d <= 5; ++d) {
            auto button = textButton(DemonNames[d - 1], d == m_demon ? "GJ_button_01.png" : "GJ_button_04.png", 44.f, this, menu_selector(GdpsRatePopup::onDemon), d, .5f);
            button->setPosition({62.f + (d - 1) * 59.f, 92.f});
            m_menu->addChild(button);
        }
    }

    auto coins = textButton(m_coins ? "Moedas: OK" : "Moedas: nao", m_coins ? "GJ_button_02.png" : "GJ_button_04.png", 90.f, this, menu_selector(GdpsRatePopup::onCoins), 0, .5f);
    coins->setPosition({298.f, 88.f});
    m_menu->addChild(coins);

    auto cancel = textButton("Cancel", "GJ_button_06.png", 60.f, this, menu_selector(GdpsRatePopup::onCancel), 0);
    cancel->setPosition({125.f, 24.f});
    m_menu->addChild(cancel);
    auto submit = textButton("Submit", "GJ_button_01.png", 60.f, this, menu_selector(GdpsRatePopup::onSubmit), 0);
    submit->setPosition({235.f, 24.f});
    m_menu->addChild(submit);
}

void GdpsRatePopup::onStar(CCObject* sender) {
    m_stars = sender->getTag();
    render();
}

void GdpsRatePopup::onFace(CCObject*) {
    if (m_stars == 0) { m_status->setString("Escolha as estrelas primeiro."); return; }
    if (!gdps::can("levels.feature")) { m_status->setString("Seu cargo nao da Featured/Epic."); return; }
    m_tier = (m_tier + 1) % 5;
    m_face->updateFeatureState(static_cast<GJFeatureState>(m_tier));
    m_tierLabel->setString(TierNames[m_tier]);
}

void GdpsRatePopup::onDemon(CCObject* sender) {
    m_demon = sender->getTag();
    render();
}

void GdpsRatePopup::onCoins(CCObject*) {
    m_coins = !m_coins;
    render();
}

void GdpsRatePopup::onCancel(CCObject*) { onClose(nullptr); }

void GdpsRatePopup::onSubmit(CCObject*) {
    if (m_busy) return;
    if (m_stars == 0) { m_status->setString("Escolha de 1 a 10 estrelas."); return; }
    m_busy = true;
    m_status->setString("Enviando...");
    auto rate = matjson::makeObject({{"levelID", m_level}, {"stars", m_stars}, {"coins", m_coins}});
    if (m_stars == 10) rate["demonDiff"] = m_demon;
    WeakRef<GdpsRatePopup> weak = this;
    m_action.admin("levels.rate", rate, [weak](gdps::Result r) {
        auto self = weak.lock();
        if (!self) return;
        if (!r.ok) { self->m_busy = false; self->m_status->setString(r.error.c_str()); return; }
        auto finish = [weak](std::string const& text) {
            auto self = weak.lock();
            if (!self) return;
            Notification::create(text, NotificationIcon::Success, 2.f)->show();
            if (self->m_done) self->m_done();
            self->onClose(nullptr);
        };
        auto summary = fmt::format("{} estrela(s), {}", self->m_stars, TierNames[self->m_tier]);
        if (!gdps::can("levels.feature")) { finish("Rate salvo: " + summary); return; }
        self->m_action.admin("levels.feature",
            matjson::makeObject({{"levelID", self->m_level}, {"feature", self->m_tier > 0}, {"epic", std::max(0, self->m_tier - 1)}}),
            [weak, finish, summary](gdps::Result f) {
                auto self = weak.lock();
                if (!self) return;
                if (!f.ok) { self->m_busy = false; self->m_status->setString(fmt::format("Estrelas salvas; destaque: {}", f.error).c_str()); return; }
                finish("Rate salvo: " + summary);
            });
    });
}

} // namespace geobr
