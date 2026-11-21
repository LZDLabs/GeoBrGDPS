#include "NatalSpectate.hpp"
#include "NatalService.hpp"
#include "../models/JsonModelUtils.hpp"

#include <Geode/binding/EndLevelLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

namespace geobr::natal {

namespace {
CCLabelBMFont* text(CCNode* parent, std::string const& s, CCPoint pos, float scale, ccColor3B color, char const* font = "chatFont.fnt", CCPoint anchor = {.5f, .5f}) {
    auto l = CCLabelBMFont::create(s.c_str(), font);
    l->setScale(scale);
    l->setColor(color);
    l->setAnchorPoint(anchor);
    l->setPosition(pos);
    parent->addChild(l);
    return l;
}
} // namespace

SpectatePopup* SpectatePopup::create() {
    auto p = new SpectatePopup;
    if (p->init()) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool SpectatePopup::init() {
    if (!Popup::init(360.f, 250.f)) return false;
    setTitle("Batalha Natalina - ao vivo");
    m_list = CCNode::create();
    m_mainLayer->addChild(m_list, 2);
    redraw();
    schedule(schedule_selector(SpectatePopup::tick), .5f);
    return true;
}

void SpectatePopup::tick(float dt) {
    m_wait += dt;
    if (m_busy || m_wait < 2.f) return;
    m_wait = 0;
    m_busy = true;
    WeakRef<SpectatePopup> weak = this;
    m_req.rpc("geobr_natal_arena_state", matjson::Value::object(), [weak](auto r) {
        auto self = weak.lock();
        if (!self) return;
        self->m_busy = false;
        if (r.success && r.value.contains("arena")) setBattle(r.value["arena"]);
        self->redraw();
    });
}

void SpectatePopup::redraw() {
    m_list->removeAllChildren();
    auto const& a = battle();
    auto state = jsonutil::stringOr(a, "state");
    auto level = a.get("level") ? jsonutil::stringOr(a["level"], "name") : std::string();
    auto winner = jsonutil::stringOr(a, "winner");
    std::string head = state == "running" ? fmt::format("{}  |  termina em {}s", level, std::max(0, static_cast<int>(secondsUntil(jsonutil::stringOr(a, "ends_at")))))
                     : state == "finished" ? std::string("Fim da batalha!") : std::string("Nenhuma batalha rolando agora");
    text(m_list, head, {180.f, 210.f}, .5f, {255, 220, 120});
    if (!winner.empty()) text(m_list, "1o lugar: " + winner, {180.f, 193.f}, .5f, {150, 255, 160});
    auto players = a["players"];
    if (!players.isArray()) return;
    int i = 0;
    for (auto const& p : players.asArray().unwrap()) {
        if (i >= 8) break;
        float y = 172.f - i * 19.f;
        bool me = jsonutil::boolOr(p, "me", false), done = jsonutil::boolOr(p, "finished", false), dq = jsonutil::boolOr(p, "disqualified", false);
        int pct = done ? 100 : static_cast<int>(jsonutil::integerOr(p, "percent"));
        ccColor3B color = me ? ccColor3B{255, 230, 120} : jsonutil::boolOr(p, "bot", false) ? ccColor3B{170, 200, 255} : ccColor3B{255, 255, 255};
        text(m_list, fmt::format("{}. {}", i + 1, jsonutil::stringOr(p, "name")), {22.f, y}, .5f, color, "chatFont.fnt", {0.f, .5f});
        auto track = CCLayerColor::create({50, 50, 60, 220}, 120.f, 7.f);
        track->setPosition({172.f, y - 3.5f});
        m_list->addChild(track);
        auto fill = CCLayerColor::create(done ? ccColor4B{120, 230, 90, 255} : ccColor4B{250, 195, 45, 255}, std::max(1.f, 1.2f * pct), 7.f);
        fill->setPosition({172.f, y - 3.5f});
        m_list->addChild(fill);
        text(m_list, dq ? std::string("DQ") : done ? std::string("PASSOU!") : fmt::format("{}%", pct), {340.f, y}, .45f, color, "chatFont.fnt", {1.f, .5f});
        ++i;
    }
}

void replayBattleLevel() {
    auto play = PlayLayer::get();
    if (!play) return;
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    EndLevelLayer* end = play->getChildByType<EndLevelLayer>(0);
    if (!end && scene) end = scene->getChildByType<EndLevelLayer>(0);
    if (end) end->onReplay(nullptr);
    else play->resetLevelFromStart();
}

void offerAfterFinish(std::string const& result, bool canSpectate, std::function<void()> onSpectate) {
    geode::createQuickPopup("Batalha Natalina",
        result + (canSpectate ? "\n\n<cy>Espectar</c> a tela dos outros ou <cl>jogar de novo</c> (nao muda o resultado)?"
                              : "\n\nQuer <cl>jogar de novo</c>? (nao muda o resultado)"),
        "Jogar de novo", canSpectate ? "Espectar" : "Fechar", [canSpectate, onSpectate](auto, bool second) {
            if (!second) { replayBattleLevel(); return; }
            if (canSpectate && onSpectate) onSpectate();
        });
}

} // namespace geobr::natal
