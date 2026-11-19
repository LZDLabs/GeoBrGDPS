#include "PresenceService.hpp"
#include "CloudService.hpp"
#include "../models/JsonModelUtils.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/ProfilePage.hpp>

#include <ctime>
#include <iomanip>
#include <sstream>

using namespace geode::prelude;

namespace geobr::presence {
namespace {

class Pinger final : public CCObject {
public:
    CloudRequest ping, read;
    void tick(float) {
        ping.rpc("geobr_presence_ping", matjson::makeObject({{"p_visibility", visibility()}}), [](auto) {});
    }
};
Pinger* s_pinger = nullptr;

// "2026-11-19T18:52:20.836018+00:00" -> seconds since then (or -1).
long long secondsSince(std::string const& iso) {
    if (iso.size() < 19) return -1;
    std::tm tm{};
    std::istringstream in(iso.substr(0, 19));
    in >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (in.fail()) return -1;
#ifdef GEODE_IS_WINDOWS
    auto then = _mkgmtime(&tm);
#else
    auto then = timegm(&tm);
#endif
    return std::max<long long>(0, static_cast<long long>(std::time(nullptr) - then));
}

std::string ago(long long s) {
    if (s < 0) return "nunca visto";
    if (s < 120) return "agora";
    if (s < 3600) return fmt::format("ha {} min", s / 60);
    if (s < 86400) return fmt::format("ha {} h", s / 3600);
    return fmt::format("ha {} dia(s)", s / 86400);
}

} // namespace

std::string visibility() {
    auto value = Mod::get()->getSettingValue<std::string>("online-status");
    if (value == "Amigos") return "friends";
    if (value == "Ninguem") return "none";
    return "all";
}

void start() {
    if (s_pinger) return;
    s_pinger = new Pinger();  // lives as long as the game
    CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(Pinger::tick), s_pinger, 120.f, false);
    s_pinger->tick(0);
}

void decorate(ProfilePage* page, GJUserScore* score) {
    if (!page || !score || score->m_accountID <= 0 || !s_pinger) return;
    int account = score->m_accountID;
    auto ids = matjson::Value::array();
    ids.push(account);
    Ref<ProfilePage> keep = page;
    s_pinger->read.rpc("geobr_presence_get", matjson::makeObject({{"p_accounts", ids}}), [keep, account](auto r) {
        auto page = keep.data();
        if (!r.success || !page->getParent() || !page->m_usernameLabel || !r.value.isArray()) return;
        auto rows = r.value.asArray().unwrap();
        if (rows.empty()) return;
        auto const& row = rows[0];
        if (jsonutil::integerOr(row, "account_id") != account) return;
        auto status = jsonutil::stringOr(row, "status", "away");
        auto seen = secondsSince(jsonutil::stringOr(row, "last_seen"));
        ccColor4F color = status == "online" ? ccColor4F{.25f, .95f, .35f, 1.f} : status == "idle" ? ccColor4F{.62f, .62f, .62f, 1.f}
            : status == "hidden" ? ccColor4F{.35f, .35f, .35f, .6f} : ccColor4F{.95f, .25f, .25f, 1.f};
        std::string text = status == "online" ? "Online agora" : status == "hidden" ? "Status oculto"
            : status == "idle" ? "Visto " + ago(seen) : "Inativo - visto " + ago(seen);
        auto visible = jsonutil::stringOr(row, "visibility");
        if (!visible.empty()) text += visible == "none" ? " (so voce ve)" : visible == "friends" ? " (amigos veem)" : "";

        auto label = page->m_usernameLabel;
        auto parent = label->getParent();
        if (!parent) return;
        if (auto old = parent->getChildByID("geobr-presence")) old->removeFromParent();
        auto node = CCNode::create();
        node->setID("geobr-presence");
        auto box = label->boundingBox();
        node->setPosition({box.getMaxX() + 9.f, box.getMidY()});
        auto dot = CCDrawNode::create();
        dot->drawDot({0, 0}, 5.f, {0, 0, 0, .7f});
        dot->drawDot({0, 0}, 3.8f, color);
        node->addChild(dot);
        auto caption = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
        caption->setScale(.45f);
        caption->setAnchorPoint({0.f, .5f});
        caption->setPosition({8.f, 0.f});
        node->addChild(caption);
        parent->addChild(node, label->getZOrder() + 1);
    });
}

} // namespace geobr::presence
