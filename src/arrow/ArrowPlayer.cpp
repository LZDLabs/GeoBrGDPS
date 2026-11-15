#include "ArrowPlayer.hpp"
#include "ArrowPortal.hpp"
#include "ArrowSkins.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/HardStreak.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <array>
#include <cmath>
#include <numbers>

using namespace geode::prelude;
using namespace geobr::arrow;

namespace {

// Number of players currently in Arrow mode. Lets non-Arrow players skip the
// field lookup on every physics step.
int g_activeArrows = 0;

constexpr int PopActionTag = 0x6A70;
constexpr float RadToDeg = 180.f / std::numbers::pi_v<float>;

float shortestDelta(float from, float to) {
    float delta = std::fmod(to - from + 540.f, 360.f) - 180.f;
    return delta;
}

// Sets a node's rotation so that its +x axis points along `worldAngle`
// (degrees, CCW) whatever rotation/mirroring its ancestors carry.
void setWorldDirection(CCNode* node, float worldAngle, float scale) {
    auto parent = node->getParent();
    if (!parent) return;
    auto origin = parent->convertToWorldSpace({0.f, 0.f});
    auto ax = parent->convertToWorldSpace({1.f, 0.f}) - origin;
    auto ay = parent->convertToWorldSpace({0.f, 1.f}) - origin;
    float det = ax.x * ay.y - ay.x * ax.y;
    if (std::abs(det) < 1e-6f) return;
    float rad = worldAngle / RadToDeg;
    float wx = std::cos(rad), wy = std::sin(rad);
    // Solve parent * local = world for the local direction.
    float lx = (ay.y * wx - ay.x * wy) / det;
    float ly = (-ax.y * wx + ax.x * wy) / det;
    node->setRotation(-std::atan2(ly, lx) * RadToDeg);
    node->setScaleX(scale);
    node->setScaleY(det < 0 ? -scale : scale);
}

} // namespace

class $modify(GeoBRArrowPlayer, PlayerObject) {
    struct Fields {
        ArrowMotor motor;
        bool internalInput = false;
        bool engineHeld = false;
        GameObject* pendingPortal = nullptr;
        int pendingAge = 0;
        GameObject* lastPortal = nullptr;
        Ref<CCNode> holder;
        Ref<ArrowIcon> icon;
        Ref<CCMotionStreak> trail;
        std::array<bool, 9> nativeVisible {};
        bool nativeSaved = false;
        bool waveTrailVisible = true;
        float angle = 0.f;
        float levelAngle = 0.f;
        bool angleReady = false;
        CCPoint lastWorld; // last position in level (object layer) space
        bool hasLastWorld = false;
    };

    // ---------------------------------------------------------- state API
    void arrowActivate(bool effects, Direction const* initial) {
        auto& f = *m_fields.operator->();
        if (!m_isDart) return;
        if (!f.motor.active()) ++g_activeArrows;
        if (initial) f.motor.activate(*initial);
        else f.motor.activateFromInput();
        syncEngineHold();
        attachVisual();
        if (effects) burst(10, {255, 190, 110}, 26.f);
    }

    void arrowDeactivate() {
        auto& f = *m_fields.operator->();
        if (!f.motor.active()) return;
        f.motor.deactivate();
        g_activeArrows = std::max(0, g_activeArrows - 1);
        detachVisual();
        syncEngineHold();
    }

    // The engine reads "jump held" as "go up": mirror the Arrow state into it
    // (or the real finger state once the Arrow is gone).
    void syncEngineHold() {
        auto& f = *m_fields.operator->();
        bool want = f.motor.active() ? f.motor.wantsEngineHeld() : f.motor.physicallyHeld();
        if (want == f.engineHeld) return;
        f.internalInput = true;
        if (want) PlayerObject::pushButton(PlayerButton::Jump);
        else PlayerObject::releaseButton(PlayerButton::Jump);
        f.internalInput = false;
        f.engineHeld = want;
    }

    // ----------------------------------------------------------- input
    bool pushButton(PlayerButton button) {
        auto& f = *m_fields.operator->();
        if (button != PlayerButton::Jump || f.internalInput) return PlayerObject::pushButton(button);
        bool consumed = f.motor.press() && m_isDart;
        if (!consumed) {
            auto result = PlayerObject::pushButton(button);
            f.engineHeld = true;
            return result;
        }
        syncEngineHold();
        onToggled();
        return true;
    }

    bool releaseButton(PlayerButton button) {
        auto& f = *m_fields.operator->();
        if (button != PlayerButton::Jump || f.internalInput) return PlayerObject::releaseButton(button);
        bool consumed = f.motor.release() && m_isDart;
        if (!consumed) {
            auto result = PlayerObject::releaseButton(button);
            f.engineHeld = false;
            return result;
        }
        return true; // releases never move the Arrow
    }

    // --------------------------------------------------------- physics
    void updateJump(float dt) {
        if (!m_isDart || g_activeArrows == 0) return PlayerObject::updateJump(dt);
        auto& f = *m_fields.operator->();
        if (!f.motor.active()) return PlayerObject::updateJump(dt);
        // The dart solver steepens mini size; the Arrow keeps 45° at any size.
        float size = m_vehicleSize;
        m_vehicleSize = 1.f;
        PlayerObject::updateJump(dt);
        m_vehicleSize = size;
        // On a slope GD's own dart solver keeps the player on the surface
        // (and climbs it, e.g. slopes made safe with D blocks): leave it.
        if (onSlope()) return;
        m_yVelocity = f.motor.velocity(m_isUpsideDown, m_yVelocity, m_playerSpeed);
    }

    bool onSlope() const { return m_isOnSlope || m_wasOnSlope; }
    bool onSurface() const { return m_isOnGround || onSlope(); }

    void update(float dt) {
        if (!m_isDart && g_activeArrows == 0) return PlayerObject::update(dt);
        auto& f = *m_fields.operator->();
        auto before = getPosition();
        PlayerObject::update(dt);
        resolvePortalSignals();
        if (f.motor.active() && !m_isDart) {
            arrowDeactivate();
            return;
        }
        // Only free flight says how the velocity sign maps to motion. On the
        // ground or a slope the collision pushes the player against its
        // velocity; RC7 counted those frames too and, after three of them,
        // flipped the mapping (the Arrow then broke on blocks and slopes).
        if (f.motor.active() && !m_isSideways && !onSurface() && !m_isDead) {
            f.motor.observeMotion(m_isUpsideDown, m_yVelocity, getPosition().y - before.y);
        }
    }

    void toggleDartMode(bool enable, bool noEffects) {
        // Restore the native sprites before GD prepares the next gamemode.
        if (!enable) arrowDeactivate();
        PlayerObject::toggleDartMode(enable, noEffects);
        if (enable) resolvePortalSignals();
    }

    void resetObject() {
        arrowDeactivate();
        PlayerObject::resetObject();
        auto& f = *m_fields.operator->();
        f.pendingPortal = nullptr;
        f.lastPortal = m_lastActivatedPortal;
        f.hasLastWorld = false;
        f.angleReady = false;
    }

    void playerDestroyed(bool noEffects) {
        PlayerObject::playerDestroyed(noEffects);
        auto& f = *m_fields.operator->();
        if (f.holder) f.holder->setVisible(false);
        if (f.trail) f.trail->setVisible(false);
    }

    // ---------------------------------------------------------- portals
    void notePortal(GameObject* object) {
        auto& f = *m_fields.operator->();
        f.pendingPortal = object;
        f.pendingAge = 0;
    }

    void resolvePortalSignals() {
        auto& f = *m_fields.operator->();
        GameObject* candidate = nullptr;
        if (f.pendingPortal) {
            candidate = f.pendingPortal;
            // GD may toggle the gamemode a few steps after announcing it.
            if (portal::isArrowPortal(candidate) && !m_isDart && ++f.pendingAge <= 8) return;
            f.pendingPortal = nullptr;
        }
        else if (m_lastActivatedPortal != f.lastPortal) {
            candidate = m_lastActivatedPortal;
        }
        f.lastPortal = m_lastActivatedPortal;
        if (!candidate || !portal::isGamemodePortal(candidate)) return;
        bool arrowPortal = portal::isArrowPortal(candidate);
        if (arrowPortal && m_isDart && !f.motor.active()) arrowActivate(true, nullptr);
        else if (!arrowPortal && f.motor.active()) arrowDeactivate();
    }

    // ---------------------------------------------------------- visuals
    std::array<CCNode*, 9> nativeSprites() {
        return {m_iconSprite, m_iconSpriteSecondary, m_iconSpriteWhitener, m_iconGlow, m_vehicleSprite,
                m_vehicleSpriteSecondary, m_vehicleSpriteWhitener, m_vehicleGlow, m_birdVehicle};
    }

    Appearance currentAppearance() {
        Appearance look;
        look.skin = ArrowSettings::skin();
        look.primary = m_playerColor1;
        look.secondary = m_playerColor2;
        look.glow = m_hasCustomGlowColor ? m_glowColor : m_playerColor2;
        look.glowEnabled = ArrowSettings::resolveGlow(m_hasGlow);
        return look;
    }

    void attachVisual() {
        auto& f = *m_fields.operator->();
        if (!f.nativeSaved) {
            auto natives = nativeSprites();
            for (size_t i = 0; i < natives.size(); ++i) f.nativeVisible[i] = natives[i] && natives[i]->isVisible();
            f.waveTrailVisible = m_waveTrail ? m_waveTrail->isVisible() : true;
            f.nativeSaved = true;
        }
        hideNatives();
        if (!f.holder) {
            // RC6-RC8 put the icon inside the player's own node tree. GD keeps
            // rotating that tree in wave mode (by the dart's angle), so on the
            // device the Arrow showed GD's rotation plus ours: 90 degrees in
            // the air. The icon now lives next to the player in the object
            // layer and follows its position every frame; its rotation comes
            // only from the Arrow's 45-degree line.
            CCNode* parent = getParent() ? getParent() : static_cast<CCNode*>(this);
            auto holder = CCNode::create();
            holder->setID("geobr-arrow-icon");
            holder->setPosition(parent == this ? CCPoint(getContentSize() / 2) : getPosition());
            parent->addChild(holder, parent == this ? 50 : getZOrder() + 1);
            auto icon = ArrowIcon::create(currentAppearance());
            holder->addChild(icon);
            f.holder = holder;
            f.icon = icon;
        }
        if (!f.trail && getParent()) {
            auto texture = Mod::get()->expandSpriteName("geobr-arrow-trail.png");
            auto trail = CCMotionStreak::create(.32f, 2.f, 9.f * std::max(.5f, m_vehicleSize), m_playerColor1, texture.c_str());
            if (trail) {
                trail->setPosition(getPosition());
                trail->setOpacity(190);
                getParent()->addChild(trail, getZOrder() - 1);
                f.trail = trail;
            }
        }
        f.holder->setVisible(true);
        f.angleReady = false;
        f.hasLastWorld = false;
        tickVisual(0.f);
    }

    void hideNatives() {
        for (auto node : nativeSprites()) if (node) node->setVisible(false);
        if (m_waveTrail) m_waveTrail->setVisible(false);
    }

    void detachVisual() {
        auto& f = *m_fields.operator->();
        if (f.nativeSaved) {
            auto natives = nativeSprites();
            for (size_t i = 0; i < natives.size(); ++i) if (natives[i]) natives[i]->setVisible(f.nativeVisible[i]);
            if (m_waveTrail) m_waveTrail->setVisible(f.waveTrailVisible);
            f.nativeSaved = false;
        }
        if (f.holder) f.holder->removeFromParent();
        f.holder = nullptr;
        f.icon = nullptr;
        if (f.trail) {
            // Let the ribbon fade instead of popping out of existence.
            f.trail->runAction(CCSequence::create(CCDelayTime::create(.35f), CCRemoveSelf::create(), nullptr));
            f.trail = nullptr;
        }
    }

    void tickVisual(float dt) {
        auto& f = *m_fields.operator->();
        if (!f.motor.active() || !f.holder || !f.icon) return;
        hideNatives();
        f.holder->setVisible(!m_isDead && isVisible());
        if (f.holder->getParent() == getParent()) {
            f.holder->setPosition(getPosition());
            if (f.holder->getZOrder() != getZOrder() + 1) f.holder->setZOrder(getZOrder() + 1);
        }
        f.icon->setAppearance(currentAppearance());

        // The Arrow always points along its 45° line. The angle is defined in
        // the level (object layer) and mapped to the screen through the
        // layer's transform: measuring on-screen motion was wrong because the
        // camera follows the player horizontally (RC6 showed ±90° in the air).
        float levelAngle = travelAngle(f.motor.direction(), m_isUpsideDown, m_isGoingLeft);
        // On a surface the Arrow lies along it, like GD's wave: flat on the
        // ground or on top of a block, along the slope on slopes (measured
        // from its own motion in level space, so any slope angle works). In
        // the air it keeps its ±45° line.
        auto levelPos = getPosition();
        bool towardSurface = f.motor.direction() == Direction::Down;
        if (onSlope()) {
            if (f.hasLastWorld) {
                auto delta = levelPos - f.lastWorld;
                if (delta.getLength() > .05f) levelAngle = std::atan2(delta.y, delta.x) * RadToDeg;
                else levelAngle = f.angleReady ? f.levelAngle : levelAngle;
            }
        }
        else if (m_isOnGround && towardSurface) {
            levelAngle = m_isGoingLeft ? 180.f : 0.f;
        }
        f.lastWorld = levelPos;
        f.hasLastWorld = true;
        f.levelAngle = levelAngle;
        float target = levelAngle;
        if (auto parent = getParent()) {
            float rad = levelAngle / RadToDeg;
            auto from = parent->convertToWorldSpace(getPosition());
            auto to = parent->convertToWorldSpace(getPosition() + CCPoint{std::cos(rad), std::sin(rad)} * 10.f);
            target = std::atan2(to.y - from.y, to.x - from.x) * RadToDeg;
        }
        if (!f.angleReady) {
            f.angle = target;
            f.angleReady = true;
        }
        else {
            f.angle += shortestDelta(f.angle, target) * std::min(1.f, dt * 24.f);
        }
        setWorldDirection(f.holder, f.angle, std::max(.3f, m_vehicleSize));
        // Upside down the art is mirrored across its own line, like GD's icons.
        if (m_isUpsideDown) f.holder->setScaleY(-f.holder->getScaleY());

        if (f.trail) {
            f.trail->setVisible(!m_isDead);
            f.trail->setPosition(getPosition());
            f.trail->setColor(m_playerColor1);
        }
    }

    void onToggled() {
        auto& f = *m_fields.operator->();
        if (f.icon) {
            f.icon->stopActionByTag(PopActionTag);
            f.icon->setScale(1.28f);
            auto pop = CCEaseBackOut::create(CCScaleTo::create(.16f, 1.f));
            pop->setTag(PopActionTag);
            f.icon->runAction(pop);
        }
        burst(5, m_playerColor2, 16.f);
    }

    void burst(int count, ccColor3B color, float distance) {
        auto parent = getParent();
        if (!parent) return;
        auto texture = Mod::get()->expandSpriteName("geobr-arrow-spark.png");
        for (int i = 0; i < count; ++i) {
            auto spark = CCSprite::create(texture.c_str());
            if (!spark) return;
            float angle = (360.f / count) * i + 12.f * (i % 3);
            float rad = angle / RadToDeg;
            spark->setPosition(getPosition());
            spark->setColor(color);
            spark->setScale(.55f * std::max(.5f, m_vehicleSize));
            spark->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            parent->addChild(spark, getZOrder() + 2);
            spark->runAction(CCSequence::create(
                CCSpawn::create(
                    CCEaseOut::create(CCMoveBy::create(.28f, {std::cos(rad) * distance, std::sin(rad) * distance}), 2.f),
                    CCFadeOut::create(.28f), CCScaleTo::create(.28f, .1f), nullptr
                ),
                CCRemoveSelf::create(), nullptr
            ));
        }
    }

    Snapshot arrowSnapshot() { return m_fields->motor.snapshot(); }

    void arrowRestore(Snapshot state) {
        auto& f = *m_fields.operator->();
        if (state.active && m_isDart) {
            if (!f.motor.active()) arrowActivate(false, &state.direction);
            f.motor.restore(state);
            syncEngineHold();
            f.angleReady = false;
            f.hasLastWorld = false;
            if (f.trail) f.trail->reset();
        }
        else {
            arrowDeactivate();
        }
    }
};

class $modify(GeoBRArrowGameLayer, GJBaseGameLayer) {
    void playerWillSwitchMode(PlayerObject* player, GameObject* object) {
        GJBaseGameLayer::playerWillSwitchMode(player, object);
        if (!player || !portal::isGamemodePortal(object)) return;
        static_cast<GeoBRArrowPlayer*>(player)->notePortal(object);
        // In Dual both players take the same portal.
        if (m_gameState.m_isDualMode) {
            for (auto other : {m_player1, m_player2}) {
                if (other && other != player) static_cast<GeoBRArrowPlayer*>(other)->notePortal(object);
            }
        }
    }

    void toggleDualMode(GameObject* object, bool dual, PlayerObject* player, bool noEffects) {
        GJBaseGameLayer::toggleDualMode(object, dual, player, noEffects);
        if (!dual || !m_player1 || !m_player2) return;
        // The spawned second player copies the first one's Arrow state.
        auto first = static_cast<GeoBRArrowPlayer*>(m_player1);
        if (first->m_fields->motor.active()) static_cast<GeoBRArrowPlayer*>(m_player2)->arrowRestore(first->arrowSnapshot());
    }

    void update(float dt) {
        GJBaseGameLayer::update(dt);
        if (g_activeArrows == 0) return;
        for (auto player : {m_player1, m_player2}) {
            if (!player) continue;
            auto arrowPlayer = static_cast<GeoBRArrowPlayer*>(player);
            auto& f = *arrowPlayer->m_fields.operator->();
            if (!f.motor.active()) continue;
            if (!player->m_isDart) {
                arrowPlayer->arrowDeactivate();
                continue;
            }
            arrowPlayer->tickVisual(dt);
        }
    }
};

namespace geobr::arrow {

bool isActive(PlayerObject* player) {
    return player && static_cast<GeoBRArrowPlayer*>(player)->m_fields->motor.active();
}

void activate(PlayerObject* player, bool effects, Direction const* initial) {
    if (player) static_cast<GeoBRArrowPlayer*>(player)->arrowActivate(effects, initial);
}

void deactivate(PlayerObject* player) {
    if (player) static_cast<GeoBRArrowPlayer*>(player)->arrowDeactivate();
}

Snapshot snapshot(PlayerObject* player) {
    return player ? static_cast<GeoBRArrowPlayer*>(player)->arrowSnapshot() : Snapshot{};
}

void restore(PlayerObject* player, Snapshot state) {
    if (player) static_cast<GeoBRArrowPlayer*>(player)->arrowRestore(state);
}

} // namespace geobr::arrow
