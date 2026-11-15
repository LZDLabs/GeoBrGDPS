#pragma once

#include <array>
#include <cmath>

// Pure gameplay rules of the Arrow gamemode. No Geode/cocos dependencies, so
// tests/ArrowPhysicsTests.cpp exercises exactly the code that ships.
namespace geobr::arrow {

// Direction is relative to the current gravity: Up means "away from the floor".
enum class Direction : unsigned char { Down = 0, Up = 1 };

constexpr Direction flipped(Direction d) {
    return d == Direction::Up ? Direction::Down : Direction::Up;
}

struct Snapshot {
    bool active = false;
    Direction direction = Direction::Down;
};

class ArrowMotor {
public:
    static constexpr double Epsilon = 1e-4;
    // Only used before the engine ever reported a dart speed (never in a
    // normal run); matches the 1x horizontal speed so the slope stays 45°.
    static constexpr double FallbackSpeedPerPlayerSpeed = 5.770002;

    bool active() const { return m_active; }
    Direction direction() const { return m_direction; }
    bool physicallyHeld() const { return m_physicallyHeld; }

    // Entering the mode keeps a held finger meaningful: holding while passing
    // the portal starts the Arrow going up, exactly once.
    void activate(Direction initial) { m_active = true; m_direction = initial; }
    void activateFromInput() { activate(m_physicallyHeld ? Direction::Up : Direction::Down); }
    void deactivate() { m_active = false; }

    Snapshot snapshot() const { return {m_active, m_direction}; }
    void restore(Snapshot s) { m_active = s.active; m_direction = s.direction; }

    // Every press inverts the direction; releases never move the Arrow.
    // Returns true when the press was consumed by the Arrow.
    bool press() {
        m_physicallyHeld = true;
        if (!m_active) return false;
        m_direction = flipped(m_direction);
        return true;
    }
    bool release() {
        m_physicallyHeld = false;
        return m_active;
    }

    // The engine's dart solver reports |vy| for a normal-size player; that is
    // exactly the 45° speed. It is cached because the engine may report zero
    // while the player rests on a surface.
    void observeEngineSpeed(double engineVelocity) {
        auto magnitude = std::abs(engineVelocity);
        if (magnitude > Epsilon) m_speed = magnitude;
    }

    // Learns how the engine's velocity sign maps to screen motion for each
    // gravity state (+1: velocity is screen space). A single step can be
    // distorted by a collision snap, so the mapping only flips after
    // RequiredVotes consecutive contrary observations.
    static constexpr int RequiredVotes = 3;
    void observeMotion(bool upsideDown, double velocity, double screenDeltaY) {
        if (std::abs(velocity) <= Epsilon || std::abs(screenDeltaY) <= Epsilon) return;
        auto slot = upsideDown ? 1 : 0;
        int observed = (velocity > 0) == (screenDeltaY > 0) ? 1 : -1;
        if (observed == m_toScreen[slot]) {
            m_contrary[slot] = 0;
            return;
        }
        if (++m_contrary[slot] >= RequiredVotes) {
            m_toScreen[slot] = observed;
            m_contrary[slot] = 0;
        }
    }
    int screenMapping(bool upsideDown) const { return m_toScreen[upsideDown ? 1 : 0]; }

    // Engine-space vertical velocity that moves the Arrow along its 45° line.
    double velocity(bool upsideDown, double engineVelocity, double playerSpeed) {
        observeEngineSpeed(engineVelocity);
        double speed = m_speed > Epsilon ? m_speed : FallbackSpeedPerPlayerSpeed * std::abs(playerSpeed);
        double awayFromFloorOnScreen = upsideDown ? -1.0 : 1.0;
        double screenSign = (m_direction == Direction::Up ? 1.0 : -1.0) * awayFromFloorOnScreen;
        return screenSign * m_toScreen[upsideDown ? 1 : 0] * speed;
    }

    // The engine reads a "held" jump as "go up". Keeping it in sync lets the
    // native dart solver, slopes and trail logic agree with the Arrow state.
    bool wantsEngineHeld() const { return m_active && m_direction == Direction::Up; }

private:
    bool m_active = false;
    bool m_physicallyHeld = false;
    Direction m_direction = Direction::Down;
    double m_speed = 0.0;
    std::array<int, 2> m_toScreen {1, 1};
    std::array<int, 2> m_contrary {0, 0};
};

// Screen-space angle (degrees, counter-clockwise from +x) of a 45° Arrow.
inline float travelAngle(Direction direction, bool upsideDown, bool goingLeft) {
    bool screenUp = (direction == Direction::Up) != upsideDown;
    float angle = screenUp ? 45.f : -45.f;
    return goingLeft ? 180.f - angle : angle;
}

} // namespace geobr::arrow
