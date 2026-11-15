#pragma once

#include <Geode/Geode.hpp>

#include <array>
#include <string>
#include <string_view>

namespace geobr::arrow {

enum class SkinSource { Free, Shop, Pass, Crate };

struct SkinInfo {
    int id;
    std::string_view name;
    SkinSource source;
    int value; // Shop: price in Tokens. Crate: rarity 1 (Comum) .. 6 (Mitica). Pass: bonus tier.
};

// Mirrors the geobr_shop_items rows "arrow-<id>" (migration RC7). Prices and
// ownership are always confirmed by the server; this table only drives UI.
inline constexpr std::array<SkinInfo, 16> Skins {{
    {1, "Flecha", SkinSource::Free, 0},
    {2, "Dardo", SkinSource::Free, 0},
    {3, "Pena", SkinSource::Free, 0},
    {4, "Cometa", SkinSource::Free, 0},
    {5, "Raio", SkinSource::Shop, 80},
    {6, "Tridente", SkinSource::Shop, 120},
    {7, "Foguete", SkinSource::Shop, 160},
    {8, "Origami", SkinSource::Shop, 200},
    {9, "Bandeira", SkinSource::Pass, 31},
    {10, "Coroa", SkinSource::Pass, 32},
    {11, "Madeira", SkinSource::Crate, 1},
    {12, "Esmeralda", SkinSource::Crate, 2},
    {13, "Safira", SkinSource::Crate, 3},
    {14, "Ametista", SkinSource::Crate, 4},
    {15, "Solar", SkinSource::Crate, 5},
    {16, "Supernova", SkinSource::Crate, 6},
}};

SkinInfo const& skinInfo(int skin);
std::string itemID(int skin);
// Free skins, or skins whose "arrow-<id>" item the server reports as owned.
bool isOwned(int skin);
std::string unlockHint(int skin);
int skinFromItem(std::string const& itemID);

enum class GlowMode : int { FollowGD = 0, On = 1, Off = 2 };

struct Appearance {
    int skin = 1;
    cocos2d::ccColor3B primary {125, 255, 0};
    cocos2d::ccColor3B secondary {0, 255, 255};
    cocos2d::ccColor3B glow {0, 255, 255};
    bool glowEnabled = false;
};

class ArrowSettings final {
public:
    static int skin();
    static void setSkin(int skin);
    static GlowMode glowMode();
    static void setGlowMode(GlowMode mode);
    // Colours of the local player as configured in the GD icon kit.
    static Appearance localAppearance();
    static bool resolveGlow(bool playerHasGlow);
    // Equips locally right away and confirms on the server (ownership is
    // validated there; a rejected skin falls back to the previous one).
    static void equip(int skin);
};

// Layered GD-style icon: glow (glow colour), primary (Col 1), secondary
// (Col 2) and an untinted detail layer. Points to +x at rotation 0.
class ArrowIcon final : public cocos2d::CCNode {
public:
    static ArrowIcon* create(Appearance const& appearance);
    void setAppearance(Appearance const& appearance);
    void setSkin(int skin);
    Appearance const& appearance() const { return m_appearance; }
    // Size of the drawn arrow in points at scale 1.
    static constexpr float BaseSize = 30.f;

private:
    bool init(Appearance const& appearance);
    void rebuild();
    Appearance m_appearance;
    cocos2d::CCSprite* m_glow = nullptr;
    cocos2d::CCSprite* m_primary = nullptr;
    cocos2d::CCSprite* m_secondary = nullptr;
    cocos2d::CCSprite* m_detail = nullptr;
};

int clampSkin(int skin);

} // namespace geobr::arrow
