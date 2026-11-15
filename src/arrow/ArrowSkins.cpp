#include "ArrowSkins.hpp"

#include "../models/JsonModelUtils.hpp"
#include "../services/CloudService.hpp"
#include "../services/GeoPassService.hpp"

#include <Geode/binding/GameManager.hpp>
#include <Geode/ui/Notification.hpp>

#include <algorithm>
#include <fmt/format.h>

using namespace geode::prelude;

namespace geobr::arrow {

int clampSkin(int skin) {
    return std::clamp(skin, 1, static_cast<int>(Skins.size()));
}

SkinInfo const& skinInfo(int skin) {
    return Skins[clampSkin(skin) - 1];
}

std::string itemID(int skin) {
    return fmt::format("arrow-{}", clampSkin(skin));
}

int skinFromItem(std::string const& id) {
    if (!id.starts_with("arrow-")) return 0;
    int value = 0;
    for (char c : id.substr(6)) {
        if (c < '0' || c > '9') return 0;
        value = value * 10 + (c - '0');
        if (value > 99) return 0;
    }
    return value >= 1 && value <= static_cast<int>(Skins.size()) ? value : 0;
}

bool isOwned(int skin) {
    auto const& info = skinInfo(skin);
    if (info.source == SkinSource::Free) return true;
    auto shop = GeoPassService::get()->state().get("shop");
    if (!shop || !shop.unwrap().isArray()) return false;
    auto id = itemID(skin);
    for (auto const& item : shop.unwrap().asArray().unwrap()) {
        if (jsonutil::stringOr(item, "id") == id) return jsonutil::boolOr(item, "owned");
    }
    return false;
}

std::string unlockHint(int skin) {
    static constexpr char const* Crates[] = {"Comum", "Incomum", "Rara", "Epica", "Lendaria", "Mitica"};
    auto const& info = skinInfo(skin);
    switch (info.source) {
        case SkinSource::Shop: return fmt::format("Loja GeoBR: {} Tokens", info.value);
        case SkinSource::Pass: return fmt::format("GeoPass: etapa bonus {}", info.value);
        case SkinSource::Crate: return fmt::format("Exclusiva da Caixa {}", Crates[std::clamp(info.value, 1, 6) - 1]);
        default: return "Gratis";
    }
}

int ArrowSettings::skin() {
    return clampSkin(Mod::get()->getSavedValue<int>("arrow-skin", 1));
}

void ArrowSettings::setSkin(int skin) {
    Mod::get()->setSavedValue("arrow-skin", clampSkin(skin));
}

void ArrowSettings::equip(int skin) {
    static geobr::CloudRequest request;
    int previous = ArrowSettings::skin();
    skin = clampSkin(skin);
    setSkin(skin);
    auto look = localAppearance();
    request.rpc("geobr_profile_arrow", matjson::makeObject({{"p_skin", skin}, {"p_glow", look.glowEnabled}}),
        [skin, previous](geobr::ApiJsonResult result) {
            if (result.success) return;
            // The server is the authority on ownership; undo a rejected equip.
            if (result.httpStatus >= 400 && result.httpStatus < 500 && result.httpStatus != 401 && result.httpStatus != 429) {
                if (ArrowSettings::skin() == skin) ArrowSettings::setSkin(previous);
                Notification::create(result.error, NotificationIcon::Warning, 3.f)->show();
            }
            else {
                log::warn("GeoBR Arrow: skin nao sincronizada ({})", result.error);
            }
        });
}

GlowMode ArrowSettings::glowMode() {
    auto raw = std::clamp(Mod::get()->getSavedValue<int>("arrow-glow-mode", 0), 0, 2);
    return static_cast<GlowMode>(raw);
}

void ArrowSettings::setGlowMode(GlowMode mode) {
    Mod::get()->setSavedValue("arrow-glow-mode", static_cast<int>(mode));
}

bool ArrowSettings::resolveGlow(bool playerHasGlow) {
    switch (glowMode()) {
        case GlowMode::On: return true;
        case GlowMode::Off: return false;
        default: return playerHasGlow;
    }
}

Appearance ArrowSettings::localAppearance() {
    Appearance look;
    look.skin = skin();
    if (auto gm = GameManager::sharedState()) {
        look.primary = gm->colorForIdx(gm->getPlayerColor());
        look.secondary = gm->colorForIdx(gm->getPlayerColor2());
        look.glow = gm->colorForIdx(gm->getPlayerGlowColor());
        look.glowEnabled = resolveGlow(gm->getPlayerGlow());
    }
    return look;
}

ArrowIcon* ArrowIcon::create(Appearance const& appearance) {
    auto node = new ArrowIcon;
    if (node->init(appearance)) {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

bool ArrowIcon::init(Appearance const& appearance) {
    if (!CCNode::init()) return false;
    setContentSize({BaseSize, BaseSize});
    setAnchorPoint({.5f, .5f});
    ignoreAnchorPointForPosition(false);
    m_appearance = appearance;
    m_appearance.skin = clampSkin(m_appearance.skin);
    rebuild();
    return true;
}

void ArrowIcon::rebuild() {
    removeAllChildrenWithCleanup(true);
    auto layer = [this](char const* part, int z) -> CCSprite* {
        auto name = Mod::get()->expandSpriteName(fmt::format("geobr-arrow-{}-{}.png", m_appearance.skin, part));
        auto sprite = CCSprite::create(name.c_str());
        if (!sprite) return nullptr;
        // Art is authored at 120px for a 30pt icon (GD UHD density).
        sprite->setScale(BaseSize / std::max(1.f, sprite->getContentWidth()));
        sprite->setPosition(getContentSize() / 2);
        addChild(sprite, z);
        return sprite;
    };
    m_glow = layer("glow", 0);
    m_primary = layer("primary", 1);
    m_secondary = layer("secondary", 2);
    m_detail = layer("detail", 3);
    setAppearance(m_appearance);
}

void ArrowIcon::setAppearance(Appearance const& appearance) {
    bool skinChanged = clampSkin(appearance.skin) != m_appearance.skin;
    m_appearance = appearance;
    m_appearance.skin = clampSkin(appearance.skin);
    if (skinChanged) {
        rebuild();
        return;
    }
    if (m_primary) m_primary->setColor(m_appearance.primary);
    if (m_secondary) m_secondary->setColor(m_appearance.secondary);
    if (m_glow) {
        m_glow->setColor(m_appearance.glow);
        m_glow->setVisible(m_appearance.glowEnabled);
    }
}

void ArrowIcon::setSkin(int skin) {
    auto look = m_appearance;
    look.skin = skin;
    setAppearance(look);
}

} // namespace geobr::arrow
