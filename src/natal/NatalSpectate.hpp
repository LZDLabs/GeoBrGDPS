#pragma once
// Batalha Natalina live view: every player's progress (updates every 2 s), for
// whoever already beat the level or did not enter.
#include "../services/CloudService.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

#include <functional>
#include <string>

namespace geobr::natal {

class SpectatePopup final : public geode::Popup {
public:
    static SpectatePopup* create();

private:
    CloudRequest m_req;
    bool m_busy = false;
    float m_wait = 0;
    cocos2d::CCNode* m_list = nullptr;

    bool init();
    void tick(float dt);
    void redraw();
};

/** "You beat it!": the result (place and Candy), then watch the others' screens or play again (does not change it). */
void offerAfterFinish(std::string const& result, bool canSpectate, std::function<void()> onSpectate);
/** Restarts the level from the end screen (or from the start). */
void replayBattleLevel();

} // namespace geobr::natal
