// Host tests for the shipped Arrow rules (src/arrow/ArrowPhysics.hpp).
// Build: c++ -std=c++20 -I src tests/ArrowPhysicsTests.cpp -o arrow_tests
#include "arrow/ArrowPhysics.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace geobr::arrow;

namespace {
int passed = 0;
#define CHECK(expr) do { assert(expr); ++passed; } while (0)
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
}

int main() {
    // 1. Every press flips; releases never move the Arrow.
    {
        ArrowMotor motor;
        motor.activate(Direction::Down);
        CHECK(motor.press());
        CHECK(motor.direction() == Direction::Up);
        CHECK(motor.release());
        CHECK(motor.direction() == Direction::Up);
        motor.press();
        CHECK(motor.direction() == Direction::Down);
        CHECK(!motor.wantsEngineHeld());
        motor.press();
        CHECK(motor.wantsEngineHeld());
    }
    // 2. Inactive motor never consumes input (other gamemodes stay vanilla).
    {
        ArrowMotor motor;
        CHECK(!motor.press());
        CHECK(!motor.release());
        CHECK(!motor.wantsEngineHeld());
    }
    // 3. Entering through the portal while holding starts going up.
    {
        ArrowMotor motor;
        motor.press();
        motor.activateFromInput();
        CHECK(motor.direction() == Direction::Up);
        ArrowMotor idle;
        idle.activateFromInput();
        CHECK(idle.direction() == Direction::Down);
    }
    // 4. Speed: the engine's normal-size dart speed is kept even when it
    //    reports zero on a surface; the 45° slope never depends on size.
    {
        ArrowMotor motor;
        motor.activate(Direction::Up);
        CHECK(near(motor.velocity(false, -5.19, .9), 5.19));
        CHECK(near(motor.velocity(false, 0.0, .9), 5.19));
        motor.press();
        CHECK(near(motor.velocity(false, 5.19, .9), -5.19));
    }
    // 5. Fallback before any engine sample uses the 1x horizontal speed.
    {
        ArrowMotor motor;
        motor.activate(Direction::Up);
        CHECK(near(motor.velocity(false, 0.0, .9), ArrowMotor::FallbackSpeedPerPlayerSpeed * .9));
    }
    // 6. Gravity: Up means away from the floor, so on screen it inverts.
    {
        ArrowMotor motor;
        motor.activate(Direction::Up);
        CHECK(motor.velocity(false, 5.0, 1.0) > 0);
        CHECK(motor.velocity(true, 5.0, 1.0) < 0); // screen-space engine (default)
    }
    // 7. If the engine turns out to be gravity-relative, the mapping is learned
    //    only after consistent evidence (a single collision snap is ignored).
    {
        ArrowMotor motor;
        motor.activate(Direction::Up);
        // Positive velocity moving the player DOWN on screen contradicts the
        // default screen-space assumption.
        motor.observeMotion(true, 5.0, -2.0);  // one contrary sample
        CHECK(motor.screenMapping(true) == 1);
        motor.observeMotion(true, 5.0, 2.0);   // agreeing sample resets the vote
        motor.observeMotion(true, 5.0, -2.0);
        motor.observeMotion(true, 5.0, -2.0);
        CHECK(motor.screenMapping(true) == 1);
        motor.observeMotion(true, 5.0, -2.0);  // third consecutive contrary sample
        CHECK(motor.screenMapping(true) == -1);
        CHECK(motor.velocity(true, 5.0, 1.0) > 0);
        CHECK(motor.screenMapping(false) == 1); // normal gravity untouched
    }
    // 8. Checkpoints restore exactly the saved state.
    {
        ArrowMotor motor;
        motor.activate(Direction::Down);
        motor.press();
        auto saved = motor.snapshot();
        motor.press();
        motor.deactivate();
        motor.restore(saved);
        CHECK(motor.active() && motor.direction() == Direction::Up);
    }
    // 9. Visual travel angle: ±45°, mirrored when going left.
    {
        CHECK(travelAngle(Direction::Up, false, false) == 45.f);
        CHECK(travelAngle(Direction::Down, false, false) == -45.f);
        CHECK(travelAngle(Direction::Up, true, false) == -45.f);
        CHECK(travelAngle(Direction::Up, false, true) == 135.f);
    }
    // 10. 45° invariant over a simulated run: |dy| == |dx| every step.
    {
        ArrowMotor motor;
        motor.activate(Direction::Down);
        double x = 0, y = 100, speed = 5.770002 * .9;
        for (int step = 0; step < 240; ++step) {
            if (step % 37 == 0) motor.press();
            double vy = motor.velocity(false, motor.direction() == Direction::Up ? speed : -speed, .9);
            double nx = x + speed, ny = y + vy;
            CHECK(near(std::abs(ny - y), std::abs(nx - x)));
            x = nx;
            y = ny;
        }
    }
    std::cout << "ArrowPhysics: " << passed << " verificacoes aprovadas\n";
}
