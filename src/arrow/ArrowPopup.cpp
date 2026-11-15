#include "ArrowPopup.hpp"
#include "ArrowPortal.hpp"
#include "../ui/GeoBRVisuals.hpp"
#include "../ui/UIHelpers.hpp"

#include <Geode/ui/Notification.hpp>

#include <cmath>

using namespace geode::prelude;

namespace geobr::arrow {
namespace {

constexpr CCSize PopupSize {470.f, 300.f};
constexpr CCSize DemoSize {196.f, 128.f};

char const* glowModeLabel(GlowMode mode) {
    switch (mode) {
        case GlowMode::On: return "Glow: Ligado";
        case GlowMode::Off: return "Glow: Desligado";
        default: return "Glow: igual ao GD";
    }
}

} // namespace

ArrowPopup* ArrowPopup::createLocal() {
    auto popup = new ArrowPopup;
    if (popup->init("Seu Arrow", ArrowSettings::localAppearance(), true)) {
        popup->autorelease();
        return popup;
    }
    delete popup;
    return nullptr;
}

ArrowPopup* ArrowPopup::createRemote(std::string const& name, Appearance const& appearance) {
    auto popup = new ArrowPopup;
    if (popup->init(fmt::format("Arrow de {}", name), appearance, false)) {
        popup->autorelease();
        return popup;
    }
    delete popup;
    return nullptr;
}

bool ArrowPopup::init(std::string owner, Appearance appearance, bool editable) {
    if (!Popup::init(PopupSize.width, PopupSize.height)) return false;
    visual::fitPopup(m_mainLayer, PopupSize);
    visual::stylePopup(m_mainLayer, m_bgSprite, PopupSize, {46, 30, 18});
    setID("geobr-arrow-popup");
    m_look = appearance;
    m_editable = editable;
    m_owner = std::move(owner);
    setTitle(m_owner, "goldFont.fnt", .7f, 18.f);

    buildDemo();
    buildCards();
    buildChannels();
    schedule(schedule_selector(ArrowPopup::tickDemo));
    return true;
}

// A small live track: the Arrow travels at 45° and every tap (or the auto
// pilot) flips the direction, the same rules used in gameplay.
void ArrowPopup::buildDemo() {
    auto panel = ui::makePanel(DemoSize, {24, 16, 10}, 255);
    panel->setPosition({30.f + DemoSize.width / 2, 160.f});
    m_mainLayer->addChild(panel);

    // Plain node (no stencil clipping: see visual::theme). Everything in the
    // demo stays inside the track by construction.
    auto clip = CCNode::create();
    clip->setContentSize({DemoSize.width - 8, DemoSize.height - 8});
    clip->setPosition({4, 4});
    panel->addChild(clip, 2);
    m_demoTrack = clip;

    // Floor/ceiling lines in GD fashion.
    for (float y : {2.f, DemoSize.height - 10.f}) {
        auto line = CCLayerColor::create({255, 255, 255, 70}, DemoSize.width - 8, 2);
        line->setPosition({0, y - 1});
        clip->addChild(line);
    }
    if (auto portal = portal::createPreview(DemoSize.height - 26.f)) {
        portal->setPosition({30.f, (DemoSize.height - 8) / 2});
        clip->addChild(portal, 1);
        m_demoPortal = portal;
    }

    m_demoTrail = CCMotionStreak::create(.45f, 1.5f, 8.f, m_look.primary, Mod::get()->expandSpriteName("geobr-arrow-trail.png").c_str());
    if (m_demoTrail) clip->addChild(m_demoTrail, 2);
    m_demoIcon = ArrowIcon::create(m_look);
    m_demoIcon->setScale(1.1f);
    m_demoY = (DemoSize.height - 8) / 2;
    m_demoIcon->setPosition({DemoSize.width / 2, m_demoY});
    clip->addChild(m_demoIcon, 3);

    auto hint = ui::makeLabel("Toque para inverter", "bigFont.fnt", .22f, ui::palette::TextSoft);
    hint->setPosition({30.f + DemoSize.width / 2, 86.f});
    m_mainLayer->addChild(hint);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 5);
    auto hit = CCNode::create();
    hit->setContentSize(DemoSize);
    auto tap = CCMenuItemSpriteExtra::create(hit, this, menu_selector(ArrowPopup::onDemoTap));
    tap->setPosition(panel->getPosition());
    menu->addChild(tap);
}

void ArrowPopup::tickDemo(float dt) {
    if (!m_demoIcon) return;
    float top = DemoSize.height - 8 - 14.f, bottom = 14.f;
    float speed = 70.f; // 45°: vertical speed equals the scroll speed
    m_demoY += m_demoDirection * speed * dt;
    if (m_demoY >= top || m_demoY <= bottom) {
        m_demoY = std::clamp(m_demoY, bottom, top);
        m_demoDirection = -m_demoDirection; // auto pilot keeps the demo alive
    }
    m_demoIcon->setPositionY(m_demoY);
    m_demoIcon->setRotation(m_demoDirection > 0 ? -45.f : 45.f);
    if (m_demoTrail) {
        m_demoTrail->setPosition({m_demoIcon->getPositionX(), m_demoY});
        m_demoTrail->setColor(m_look.primary);
    }
    // The world scrolls left so the Arrow reads as moving right at 45°.
    if (m_demoPortal) {
        m_demoPortal->setPositionX(m_demoPortal->getPositionX() - speed * dt);
        if (m_demoPortal->getPositionX() < -20.f) m_demoPortal->setPositionX(DemoSize.width + 20.f);
    }
}

void ArrowPopup::onDemoTap(CCObject*) {
    m_demoDirection = -m_demoDirection;
    if (m_demoIcon) {
        m_demoIcon->stopAllActions();
        m_demoIcon->setScale(1.4f);
        m_demoIcon->runAction(CCEaseBackOut::create(CCScaleTo::create(.16f, 1.1f)));
    }
}

void ArrowPopup::buildCards() {
    if (m_cards) m_cards->removeFromParent();
    m_cards = CCNode::create();
    m_mainLayer->addChild(m_cards, 4);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_cards->addChild(menu);
    // 4 x 4 grid: free, shop, pass and crate-exclusive skins.
    constexpr float CellW = 50.f, CellH = 44.f;
    constexpr CCPoint Origin {262.f, 238.f};
    for (size_t i = 0; i < Skins.size(); ++i) {
        auto const& skin = Skins[i];
        bool selected = m_look.skin == skin.id;
        bool owned = !m_editable || isOwned(skin.id);
        auto card = CCNodeRGBA::create();
        card->setContentSize({CellW - 4, CellH - 4});
        card->setCascadeOpacityEnabled(true);
        auto frame = ui::Frame::create({CellW - 4, CellH - 4}, selected ? ccColor3B{150, 92, 40} : ccColor3B{48, 34, 26}, 255, false);
        if (selected) frame->setBorderColor({255, 220, 120});
        frame->setPosition(card->getContentSize() / 2);
        card->addChild(frame);
        auto look = m_look;
        look.skin = skin.id;
        if (!owned) {
            look.primary = {90, 90, 90};
            look.secondary = {150, 150, 150};
            look.glowEnabled = false;
        }
        auto icon = ArrowIcon::create(look);
        icon->setScale(.9f);
        icon->setPosition(card->getContentSize() / 2);
        card->addChild(icon, 2);
        if (!owned) {
            if (auto lock = CCSprite::createWithSpriteFrameName("GJ_lock_001.png")) {
                lock->setScale(.35f);
                lock->setPosition(card->getContentSize() / 2);
                card->addChild(lock, 3);
            }
        }
        auto button = ui::Button::create(card, this, menu_selector(ArrowPopup::onSkin));
        button->setTag(skin.id);
        button->setPosition(Origin + CCPoint{(i % 4) * CellW, -static_cast<float>(i / 4) * CellH});
        button->setEnabled(m_editable || selected);
        menu->addChild(button);
    }
    auto name = ui::makeLabel(fmt::format("{} | {}", skinInfo(m_look.skin).name, unlockHint(m_look.skin)), "bigFont.fnt", .26f, ui::palette::TextGold);
    name->setPosition({Origin.x + CellW * 1.5f, Origin.y - CellH * 4 + 14.f});
    name->limitLabelWidth(210.f, .26f, .15f);
    m_cards->addChild(name);
}

void ArrowPopup::buildChannels() {
    float y = 54.f;
    struct Channel { char const* label; ccColor3B color; bool on; };
    std::array<Channel, 3> channels {{
        {"Col 1", m_look.primary, true},
        {"Col 2", m_look.secondary, true},
        {"Glow", m_look.glow, m_look.glowEnabled},
    }};
    for (size_t i = 0; i < channels.size(); ++i) {
        float x = 50.f + i * 70.f;
        auto chip = ui::Frame::create({22, 22}, channels[i].color, channels[i].on ? 255 : 90, false);
        chip->setPosition({x, y});
        m_mainLayer->addChild(chip);
        auto label = ui::makeLabel(channels[i].label, "bigFont.fnt", .24f, ui::palette::TextSoft);
        label->setAnchorPoint({0, .5f});
        label->setPosition({x + 15.f, y});
        m_mainLayer->addChild(label);
    }
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 5);
    if (m_editable) {
        auto glow = ui::makeTextButton(glowModeLabel(ArrowSettings::glowMode()), 170, "GJ_button_04.png", this,
            menu_selector(ArrowPopup::onGlowMode), .3f, 26.f);
        glow->setPosition({357.f, 54.f});
        menu->addChild(glow);
    }
    m_status = ui::makeLabel(
        m_editable ? "Cores seguem o icon kit: Col 1, Col 2 e Glow" : "Visualizacao do Arrow deste jogador",
        "chatFont.fnt", .45f, ui::palette::TextSoft
    );
    m_status->setPosition({PopupSize.width / 2, 22.f});
    m_status->limitLabelWidth(430.f, .45f, .25f);
    m_mainLayer->addChild(m_status);
}

void ArrowPopup::onSkin(CCObject* sender) {
    if (!m_editable || !sender) return;
    int skin = clampSkin(sender->getTag());
    if (!isOwned(skin)) {
        m_status->setString(fmt::format("{}: {}", skinInfo(skin).name, unlockHint(skin)).c_str());
        return;
    }
    m_look.skin = skin;
    ArrowSettings::equip(skin);
    if (m_demoIcon) m_demoIcon->setSkin(m_look.skin);
    buildCards();
    m_status->setString(fmt::format("Skin {} equipada", skinInfo(skin).name).c_str());
}

void ArrowPopup::onGlowMode(CCObject* sender) {
    auto next = static_cast<GlowMode>((static_cast<int>(ArrowSettings::glowMode()) + 1) % 3);
    ArrowSettings::setGlowMode(next);
    m_look = ArrowSettings::localAppearance();
    if (m_demoIcon) m_demoIcon->setAppearance(m_look);
    // Rebuild the channel row and the button label.
    if (auto button = typeinfo_cast<CCMenuItemSpriteExtra*>(sender)) {
        auto parent = button->getParent();
        auto position = button->getPosition();
        button->removeFromParent();
        auto glow = ui::makeTextButton(glowModeLabel(next), 170, "GJ_button_04.png", this, menu_selector(ArrowPopup::onGlowMode), .3f, 26.f);
        glow->setPosition(position);
        parent->addChild(glow);
    }
    buildCards();
    ArrowSettings::equip(m_look.skin);
}


} // namespace geobr::arrow
