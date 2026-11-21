#include "NatalService.hpp"
#include "../gdps/GdpsService.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/CloudService.hpp"
#include "../services/MainLevelLauncher.hpp"
#include "../services/MainLevelProgress.hpp"

#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/CCSpritePart.hpp>
#include <Geode/binding/GJSpiderSprite.hpp>
#include <Geode/binding/GJRobotSprite.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/binding/FLAlertLayer.hpp>

#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

using namespace geode::prelude;

namespace geobr::natal {
namespace {

matjson::Value s_state = matjson::Value::object();
matjson::Value s_catalog = matjson::Value::array();
matjson::Value s_levels = matjson::Value::array();
matjson::Value s_cosmetics = matjson::Value::array();
matjson::Value s_battle = matjson::Value::object();
double s_offset = 0;
CloudRequest* s_stateReq = nullptr;
CloudRequest* s_catalogReq = nullptr;
CloudRequest* s_levelsReq = nullptr;
CloudRequest* s_cosmeticsReq = nullptr;

CloudRequest& req(CloudRequest*& slot) {
    if (!slot) slot = new CloudRequest();  // lives as long as the game
    return *slot;
}

double epochOf(std::string const& iso) {
    if (iso.size() < 19) return 0;
    std::tm tm{};
    std::istringstream in(iso.substr(0, 19));
    in >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (in.fail()) return 0;
#ifdef GEODE_IS_WINDOWS
    return static_cast<double>(_mkgmtime(&tm));
#else
    return static_cast<double>(timegm(&tm));
#endif
}

} // namespace

matjson::Value const& state() { return s_state; }
matjson::Value const& accessories() { return s_catalog; }
matjson::Value const& levels() { return s_levels; }
matjson::Value const& cosmetics() { return s_cosmetics; }
matjson::Value const* cosmetic(std::string const& id) {
    if (!s_cosmetics.isArray()) return nullptr;
    for (auto const& c : s_cosmetics.asArray().unwrap()) if (jsonutil::stringOr(c, "id") == id) return &c;
    return nullptr;
}
bool ownsCosmetic(std::string const& id) {
    auto list = s_state.get("cosmetics");
    if (!list || !list.unwrap().isArray()) return false;
    for (auto const& c : list.unwrap().asArray().unwrap()) if (c.asString().unwrapOr("") == id) return true;
    return false;
}
matjson::Value const& battle() { return s_battle; }

matjson::Value const* accessory(std::string const& id) {
    if (!s_catalog.isArray()) return nullptr;
    for (auto const& a : s_catalog.asArray().unwrap()) if (jsonutil::stringOr(a, "id") == id) return &a;
    return nullptr;
}

matjson::Value const* level(std::int64_t id) {
    if (!s_levels.isArray()) return nullptr;
    for (auto const& l : s_levels.asArray().unwrap()) if (jsonutil::integerOr(l, "id") == id) return &l;
    return nullptr;
}

std::string equipped() {
    auto me = s_state.get("me");
    return me ? jsonutil::stringOr(me.unwrap(), "equipped") : std::string();
}

bool active() {
    auto event = s_state.get("event");
    return event && jsonutil::boolOr(event.unwrap(), "active", false);
}

bool isAdmin() { return jsonutil::boolOr(s_state, "is_admin", false); }

void setState(matjson::Value const& next) {
    if (!next.isObject() || !next.contains("me")) return;
    s_state = next;
    auto event = next.get("event");
    if (event) {
        auto server = epochOf(jsonutil::stringOr(event.unwrap(), "now"));
        if (server > 0) s_offset = server - static_cast<double>(std::time(nullptr));
    }
    // Decoration follows the event being open for this player (public, or staff test mode).
    Mod::get()->setSavedValue("natal-enabled", event && jsonutil::boolOr(event.unwrap(), "active", false));
    Mod::get()->setSavedValue("natal-equipped", equipped());
}

void refresh(Done done) {
    auto pending = std::make_shared<int>(4);
    auto failed = std::make_shared<std::string>();
    auto finish = [pending, failed, done](std::string const& error) {
        if (!error.empty() && failed->empty()) *failed = error;
        if (--*pending == 0 && done) done(failed->empty(), *failed);
    };
    req(s_stateReq).rpc("geobr_natal_state", matjson::Value::object(), [finish](auto r) {
        if (r.success) setState(r.value);
        finish(r.success ? std::string() : r.error);
    });
    req(s_catalogReq).call("GET", "/rest/v1/geobr_natal_accessories?published=eq.true&order=sort_order", {}, [finish](auto r) {
        if (r.success && r.value.isArray()) s_catalog = r.value;
        finish(r.success ? std::string() : r.error);
    });
    req(s_cosmeticsReq).call("GET", "/rest/v1/geobr_natal_cosmetics?published=eq.true&order=sort_order", {}, [finish](auto r) {
        if (r.success && r.value.isArray()) s_cosmetics = r.value;
        finish(r.success ? std::string() : r.error);
    });
    req(s_levelsReq).call("GET", "/rest/v1/geobr_natal_levels?published=eq.true&order=kind,sort_order,id", {}, [finish](auto r) {
        if (r.success && r.value.isArray()) s_levels = r.value;
        finish(r.success ? std::string() : r.error);
    });
}

bool decorationsOn() {
    return gdps::enabled() && Mod::get()->getSettingValue<bool>("natal-decoracao") && Mod::get()->getSavedValue<bool>("natal-enabled", false);
}

void applyTexturePack() {
    if (!decorationsOn()) return;
    auto dir = Mod::get()->getConfigDir() / "texturas-natal";
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    if (std::filesystem::is_empty(dir, error)) return;
    CCFileUtils::get()->addPriorityPath(utils::string::pathToString(dir).c_str());
    log::info("Natal: texture folder in use ({})", utils::string::pathToString(dir));
}

MainLevel mainLevelOf(matjson::Value const& row) {
    MainLevel level;
    level.id = LevelIdOffset + jsonutil::integerOr(row, "id");
    level.order = static_cast<int>(jsonutil::integerOr(row, "sort_order", 1));
    level.name = jsonutil::stringOr(row, "name", "Fase de Natal");
    level.creator = jsonutil::stringOr(row, "creator", "Equipe GeoBR");
    level.difficulty = jsonutil::stringOr(row, "difficulty", "Normal");
    level.description = jsonutil::stringOr(row, "description");
    level.song = jsonutil::stringOr(row, "song");
    level.gmdPath = jsonutil::stringOr(row, "gmd_path");
    level.stars = static_cast<int>(jsonutil::integerOr(row, "stars"));
    level.fileVersion = static_cast<int>(jsonutil::integerOr(row, "file_version", 1));
    level.platformer = jsonutil::boolOr(row, "platformer", false);
    level.published = true;
    return level;
}

std::int64_t eventLevelFromLocal(int localLevelID) {
    std::int64_t base = static_cast<std::int64_t>(MainLevelProgress::LocalIDBase) + LevelIdOffset;
    std::int64_t id = static_cast<std::int64_t>(localLevelID) - base;
    return id > 0 && id < 1'000'000 ? id : 0;
}

AccessoryArt makeAccessory(std::string const& id, float unit) {
    AccessoryArt out;
    if (id.empty()) return out;
    auto a = accessory(id);
    if (!a) return out;
    auto sprite = CCSprite::create(Mod::get()->expandSpriteName(jsonutil::stringOr(*a, "sprite")).data());
    if (!sprite) return out;
    auto size = sprite->getContentSize();
    float longest = std::max(1.f, std::max(size.width, size.height));
    float scale = static_cast<float>((*a)["scale"].asDouble().unwrapOr(1.0));
    sprite->setScale(unit * scale / longest);
    out.sprite = sprite;
    out.offset = CCPoint(static_cast<float>((*a)["off_x"].asDouble().unwrapOr(0.0)) * unit, static_cast<float>((*a)["off_y"].asDouble().unwrapOr(0.0)) * unit);
    out.behind = jsonutil::boolOr(*a, "behind", false);
    return out;
}

void attach(CCNode* icon, std::string const& id, float unit) {
    if (!icon) return;
    if (auto old = icon->getChildByID("geobr-natal-accessory")) old->removeFromParent();
    // Remembered so the icon kit can move it when the shown mode changes.
    icon->setUserObject("geobr.natal-accessory", CCString::create(id));
    auto art = makeAccessory(id, unit);
    if (!art.sprite) return;
    auto box = icon->getContentSize();
    CCPoint center{box.width / 2.f, box.height / 2.f};
    float k = 1.f;
    if (auto simple = typeinfo_cast<SimplePlayer*>(icon)) {
        auto stored = typeinfo_cast<CCInteger*>(simple->getUserObject("geobr.icon-type"));
        auto type = stored ? static_cast<IconType>(stored->getValue()) : IconType::Cube;
        // Ship, UFO, wave, swing and jetpack previews have no head to wear it.
        if (type == IconType::Ship || type == IconType::Ufo || type == IconType::Wave || type == IconType::Swing || type == IconType::Jetpack) return;
        // Robot and spider: on the head part, sized to it (the head counts as the cube).
        CCSprite* head = nullptr;
        if (type == IconType::Robot && simple->m_robotSprite) head = simple->m_robotSprite->m_headSprite;
        if (type == IconType::Spider && simple->m_spiderSprite) head = simple->m_spiderSprite->m_headSprite;
        if (head) {
            auto cs = head->getContentSize();
            CCPoint c = icon->convertToNodeSpace(head->convertToWorldSpace(cs / 2));
            CCPoint top = icon->convertToNodeSpace(head->convertToWorldSpace({cs.width / 2.f, cs.height}));
            float half = std::max(1.f, (top - c).getLength());
            k = half * 2.f / unit;
            center = c;
        }
    }
    art.sprite->setScale(art.sprite->getScale() * k);
    art.sprite->setPosition(center + art.offset * k);
    art.sprite->setID("geobr-natal-accessory");
    icon->addChild(art.sprite, art.behind ? -5 : 60);
}

void setBattle(matjson::Value const& next) {
    s_battle = next.isObject() ? next : matjson::Value::object();
    if (auto now = s_battle.get("now")) {
        auto server = epochOf(now.unwrap().asString().unwrapOr(""));
        if (server > 0) s_offset = server - static_cast<double>(std::time(nullptr));
    }
}

std::string battleId() {
    if (auto id = s_battle.get("id"); id && id.unwrap().isNumber()) return std::to_string(id.unwrap().asInt().unwrapOr(0));
    return jsonutil::stringOr(s_battle, "id");
}
double serverOffset() { return s_offset; }

double secondsUntil(std::string const& iso) {
    auto at = epochOf(iso);
    if (at <= 0) return 0;
    return at - (static_cast<double>(std::time(nullptr)) + s_offset);
}

// --------------------------------------------------------------- battle watch
namespace {
CloudRequest* s_watchReq = nullptr;
geode::async::TaskHolder<geode::utils::web::WebResponse>* s_launch = nullptr;
bool s_watchBusy = false;
double s_nextPoll = 0, s_nextRefresh = 0;
std::string s_notifiedArena, s_launchedArena, s_startNotified, s_resultNotified;
bool s_spectateNext = false;
int s_fired = 0;   // bits: 1 = 1 h, 2 = 5 min, 4 = 1 min

double steadyNow() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

void notify(std::string const& text, NotificationIcon icon = NotificationIcon::Info) {
    Notification::create(text, icon, 4.f)->show();
}

// Forbidden mods this game has on (from the battle's BattleGuard list).
std::vector<std::string> blockedMods(matjson::Value const& arena) {
    std::vector<std::string> out;
    auto guard = arena["guard"];
    for (auto const& mine : enabledMods()) {
        if (mine == "geobr.speedhack") { out.push_back("speedhack"); continue; }
        if (!guard.isArray()) continue;
        for (auto const& g : guard.asArray().unwrap()) {
            auto id = g.asString().unwrapOr("");
            if (!id.empty() && geode::utils::string::toLower(id) == geode::utils::string::toLower(mine)) {
                auto mod = Loader::get()->getLoadedMod(mine);
                out.push_back(mod ? std::string(mod->getName()) : mine);
            }
        }
    }
    return out;
}

std::string s_guardWarned;

// BattleGuard warns before the battle (once per battle): turn the mod off now,
// or the entry refuses you; no error in the middle of a run.
void warnGuard(matjson::Value const& arena, double left) {
    auto id = battleId();
    if (s_guardWarned == id) return;
    auto blocked = blockedMods(arena);
    if (blocked.empty()) return;
    s_guardWarned = id;
    std::string list;
    for (auto const& m : blocked) list += (list.empty() ? "" : ", ") + m;
    auto when = left > 90 ? fmt::format("em {} min", static_cast<int>(std::lround(left / 60.0))) : std::string("em instantes");
    if (PlayLayer::get() || LevelEditorLayer::get()) {
        notify("BattleGuard: desative " + list + " antes da Batalha Natalina (" + when + ")", NotificationIcon::Warning);
        return;
    }
    FLAlertLayer::create("BattleGuard",
        fmt::format("A <cy>Batalha Natalina</c> comeca {}.\n\nDesative <cr>{}</c> em <cg>Geode > Mods</c> e reinicie o jogo, "
                    "senao voce <cr>nao consegue entrar</c> antes da partida comecar.", when, list),
        "Entendi")->show();
}

// Notifications before the start; returns seconds until the next poll.
// Hourly (automatic) battles only warn at 5 min and 1 min.
double watchLobby(matjson::Value const& arena) {
    auto id = battleId();
    if (id != s_notifiedArena) { s_notifiedArena = id; s_fired = 0; }
    double left = secondsUntil(jsonutil::stringOr(arena, "lobby_until"));
    bool joined = jsonutil::boolOr(arena, "joined", false);
    bool autoBattle = jsonutil::boolOr(arena, "auto", false);
    constexpr double marks[] = {3600, 300, 60};
    int due = 0;
    for (int i = autoBattle ? 1 : 0; i < 3; ++i) if (left <= marks[i] + 30 && left > 0) due |= 1 << i;
    if (due & ~s_fired) {
        s_fired |= due;
        if (left > 50 * 60) notify("Batalha Natalina em 1 hora! Quem passar a fase primeiro ganha.");
        else if (left > 90) notify(fmt::format("Batalha Natalina em {} minutos! Fique ligado.", static_cast<int>(std::lround(left / 60.0))));
        else notify(joined ? std::string("Batalha Natalina em 1 minuto! Voce ja esta dentro.")
                           : std::string("Batalha Natalina em 1 minuto! Entre: GeoBR > Natal > Batalha"), NotificationIcon::Success);
        warnGuard(arena, left);
    }
    if (left > 3700) return 60;
    if (left > 330) return 20;
    return joined ? 2 : 5;
}

double watchArena(matjson::Value const& arena) {
    auto state = jsonutil::stringOr(arena, "state");
    if (state == "lobby") return watchLobby(arena);
    // Result of a battle you played: place and Candy won.
    if (state == "finished" && jsonutil::boolOr(arena, "joined", false) && s_resultNotified != battleId()) {
        s_resultNotified = battleId();
        auto players = arena["players"];
        if (players.isArray()) for (auto const& p : players.asArray().unwrap()) {
            if (!jsonutil::boolOr(p, "me", false)) continue;
            auto place = jsonutil::integerOr(p, "place"), prize = jsonutil::integerOr(p, "prize");
            notify(prize > 0 ? fmt::format("Batalha encerrada! Voce ficou em {}o e ganhou {} Candy", place, prize)
                             : fmt::format("Batalha encerrada! Voce ficou em {}o lugar", place), NotificationIcon::Success);
        }
    }
    if (state != "running" || !jsonutil::boolOr(arena, "joined", false)) return state == "running" ? 10 : 30;
    bool loading = jsonutil::boolOr(arena, "loading", false);
    bool inLevel = PlayLayer::get() || LevelEditorLayer::get();
    auto id = battleId();
    if (loading && !jsonutil::boolOr(arena, "my_ready", false) && s_launchedArena != id) {
        if (inLevel) {
            if (s_startNotified != id) { s_startNotified = id; notify("A Batalha Natalina comecou! Saia desta fase para jogar.", NotificationIcon::Warning); }
        } else {
            s_launchedArena = id;
            notify("A Batalha Natalina comecou! Abrindo a fase...", NotificationIcon::Success);
            openBattleLevel();
        }
    }
    return loading ? 2 : 10;
}

void watchTick() {
    if (!gdps::enabled()) return;
    double t = steadyNow();
    if (!active()) {
        if (t >= s_nextRefresh) { s_nextRefresh = t + 300; refresh(); }
        return;
    }
    if (s_watchBusy || t < s_nextPoll) return;
    s_watchBusy = true;
    req(s_watchReq).rpc("geobr_natal_arena_state", matjson::Value::object(), [](auto r) {
        s_watchBusy = false;
        double t = steadyNow();
        if (!r.success) { s_nextPoll = t + 30; return; }
        auto arena = r.value.contains("arena") && r.value["arena"].isObject() ? r.value["arena"] : matjson::Value::object();
        setBattle(arena);
        s_nextPoll = t + watchArena(arena);
    });
}

class BattleWatch final : public CCObject {
public:
    void tick(float) { watchTick(); }
};
} // namespace

bool takeSpectateRequest() {
    bool r = s_spectateNext;
    s_spectateNext = false;
    return r;
}

void openBattleLevel(bool spectate) {
    s_spectateNext = spectate;
    auto const& b = s_battle;
    if (!b.get("level") || !b["level"].isObject()) return;
    auto id = jsonutil::integerOr(b["level"], "id");
    auto row = level(id) ? *level(id) : b["level"];
    auto main = mainLevelOf(row);
    if (auto local = MainLevelLauncher::cached(main); local) {
        CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(.3f, PlayLayer::scene(local.level, false, false)));
        return;
    }
    if (!s_launch) s_launch = new geode::async::TaskHolder<geode::utils::web::WebResponse>();
    s_launch->spawn("GeoBR - batalha de Natal", MainLevelLauncher::download(main), [main](auto response) {
        auto imported = MainLevelLauncher::import(main, response);
        if (!imported) { notify("Batalha: " + imported.error, NotificationIcon::Error); return; }
        CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(.3f, PlayLayer::scene(imported.level, false, false)));
    });
}

void startBattleWatch() {
    static BattleWatch* watch = nullptr;
    if (watch) return;
    watch = new BattleWatch();
    CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(BattleWatch::tick), watch, 1.f, false);
}

std::vector<std::string> enabledMods() {
    std::vector<std::string> out;
    for (auto mod : Loader::get()->getAllMods()) if (mod && mod->isLoaded()) out.push_back(mod->getID());
    // A sped-up (or slowed) game counts as a forbidden mod in the battle.
    if (std::abs(CCDirector::sharedDirector()->getScheduler()->getTimeScale() - 1.f) > .01f) out.push_back("geobr.speedhack");
    return out;
}

std::string rarityName(std::string const& r) {
    if (r == "comum") return "Comum";
    if (r == "incomum") return "Incomum";
    if (r == "raro") return "Raro";
    if (r == "epico") return "Epico";
    if (r == "lendario") return "Lendario";
    if (r == "extremamente_raro") return "Extremamente Raro";
    return r;
}

ccColor3B rarityColor(std::string const& r) {
    if (r == "incomum") return {90, 220, 110};
    if (r == "raro") return {80, 150, 255};
    if (r == "epico") return {200, 110, 255};
    if (r == "lendario") return {255, 185, 50};
    if (r == "extremamente_raro") return {255, 70, 70};
    return {200, 200, 200};
}

std::string rewardText(matjson::Value const& reward) {
    auto kind = jsonutil::stringOr(reward, "kind");
    if (kind == "candy") return fmt::format("{} Candy", jsonutil::integerOr(reward, "amount"));
    if (kind == "box") return "Caixa " + rarityName(jsonutil::stringOr(reward, "rarity"));
    if (kind == "accessory") {
        auto a = accessory(jsonutil::stringOr(reward, "id"));
        return a ? jsonutil::stringOr(*a, "name", "Acessorio") : std::string("Acessorio exclusivo");
    }
    if (kind == "theme" || kind == "frame") {
        auto c = cosmetic(jsonutil::stringOr(reward, "id"));
        return (kind == "theme" ? std::string("Tema: ") : std::string("Moldura: ")) + (c ? jsonutil::stringOr(*c, "name") : jsonutil::stringOr(reward, "id"));
    }
    return "Premio";
}

} // namespace geobr::natal
