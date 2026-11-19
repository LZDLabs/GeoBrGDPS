// "Levar pro GDPS GeoBR": in the normal GD (official servers) your own level
// gets a button that keeps a copy (.gmd, made by gmd-api) in the mod's save
// folder, which both servers share. The next time the game opens in the GDPS,
// the copies become levels in "Create" and the files move to "importados".
#include "../gdps/GdpsService.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LocalLevelManager.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <hjfod.gmd-api/include/GMD.hpp>

#include <ctime>
#include <filesystem>

using namespace geode::prelude;

namespace {

std::filesystem::path transferDir() { return Mod::get()->getSaveDir() / "levar-gdps"; }

std::string safeName(std::string name) {
    for (auto& c : name) if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
    return name.substr(0, 40);
}

void importTransfers() {
    auto dir = transferDir();
    std::error_code error;
    if (!std::filesystem::exists(dir, error)) return;
    auto done = dir / "importados";
    std::filesystem::create_directories(done, error);
    int count = 0;
    std::string last;
    for (auto const& entry : std::filesystem::directory_iterator(dir, error)) {
        std::error_code e;
        if (!entry.is_regular_file(e) || entry.path().extension() != ".gmd") continue;
        auto imported = gmd::importGmdAsLevel(entry.path());
        if (imported.isErr() || !imported.unwrap()) {
            log::warn("Levar pro GDPS: {} nao importou: {}", entry.path().filename().string(), imported.isErr() ? imported.unwrapErr() : "vazio");
            continue;
        }
        auto level = imported.unwrap();
        level->m_levelType = GJLevelType::Editor;
        if (auto levels = LocalLevelManager::sharedState()->m_localLevels) levels->insertObject(level, 0);
        std::filesystem::rename(entry.path(), done / entry.path().filename(), e);
        if (e) std::filesystem::remove(entry.path(), e);
        ++count;
        last = level->m_levelName;
    }
    if (count == 1) Notification::create(fmt::format("\"{}\" chegou do GD normal! Veja em Criar.", last), NotificationIcon::Success, 4.f)->show();
    else if (count > 1) Notification::create(fmt::format("{} levels chegaram do GD normal! Veja em Criar.", count), NotificationIcon::Success, 4.f)->show();
}

} // namespace

// Normal GD: stays active outside the GDPS (same hook priority as the server menu).
class $modify(GeoBRSendToGdps, EditLevelLayer) {
    static void onModify(auto& self) { (void)self.setHookPriority("EditLevelLayer::init", geobr::gdps::ServerMenuHookPriority); }

    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) return false;
        if (geobr::gdps::enabled() || !level) return true;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto sprite = ButtonSprite::create("Levar pro GDPS", 90, true, "bigFont.fnt", "GJ_button_02.png", 26, .5f);
        sprite->setScale(.6f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(GeoBRSendToGdps::onSendToGdps));
        button->setID("geobr-send-to-gdps");
        auto menu = CCMenu::create();
        menu->setID("geobr-send-to-gdps-menu");
        menu->setPosition({win.width - 62.f, 22.f});
        menu->addChild(button);
        addChild(menu, 10);
        return true;
    }

    void onSendToGdps(CCObject*) {
        if (!m_level) return;
        auto bytes = gmd::ExportGmdFile::from(m_level).intoBytes();
        if (bytes.isErr()) {
            FLAlertLayer::create("GDPS GeoBR", fmt::format("Nao deu para copiar o level: {}", bytes.unwrapErr()), "OK")->show();
            return;
        }
        std::error_code error;
        std::filesystem::create_directories(transferDir(), error);
        auto path = transferDir() / fmt::format("{}-{}.gmd", std::time(nullptr), safeName(m_level->m_levelName));
        if (auto written = file::writeBinary(path, bytes.unwrap()); written.isErr()) {
            FLAlertLayer::create("GDPS GeoBR", fmt::format("Nao deu para salvar a copia: {}", written.unwrapErr()), "OK")->show();
            return;
        }
        FLAlertLayer::create("GDPS GeoBR",
            fmt::format("<cg>{}</c> foi guardado!\nQuando voce entrar no <cy>GDPS GeoBR</c>, ele aparece nos seus levels (Criar).", std::string(m_level->m_levelName)),
            "OK")->show();
    }
};

// GDPS: bring the copies in when the main menu opens.
class $modify(GeoBRReceiveFromVanilla, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        if (geobr::gdps::enabled()) importTransfers();
        return true;
    }
};
