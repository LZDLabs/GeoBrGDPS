#include "GodEditor.hpp"
#include "../arrow/ArrowPortal.hpp"
#include "../ui/GeoBRVisuals.hpp"
#include "../ui/UIHelpers.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LocalLevelManager.hpp>
#include <Geode/binding/ObjectToolbox.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {

constexpr int OverlayPriority = -400;   // above gameplay touches, below popups
constexpr int OverlayMenuPriority = -401;
constexpr int ArrowPaletteID = -1;     // palette entry for the brown portal
constexpr float FlySpeed = 300.f;      // level units per second (10 blocks/s)
constexpr char OverlayID[] = "geobr-god-editor";

GJBaseGameLayer* g_active = nullptr;

struct PaletteTab {
    char const* name;
    std::vector<int> ids;
};

// Curated categories; anything else is reachable by ID ("qualquer coisa do editor").
std::vector<PaletteTab> const& paletteTabs() {
    static std::vector<PaletteTab> const tabs {
        {"Blocos", {1, 2, 3, 4, 5, 6, 7, 40, 62, 83, 289, 291}},
        {"Perigos", {8, 39, 103, 9, 88, 89, 98, 397}},
        {"Pulos", {35, 140, 67, 1332, 36, 141, 84, 1022, 1330, 1333}},
        {"Portais", {ArrowPaletteID, 12, 13, 47, 111, 660, 745, 1331, 1933, 10, 11, 99, 101, 200, 201, 202, 203, 286, 287}},
        {"Deco", {503, 504, 505, 18, 19, 20, 21, 41, 106, 107, 1764}},
    };
    return tabs;
}

bool objectExists(int id) {
    if (id == ArrowPaletteID) return true;
    if (id <= 0 || id > 5000) return false;
    auto frame = ObjectToolbox::sharedState()->intKeyToFrame(id);
    return frame && frame[0] != '\0';
}

CCNode* objectPreview(int id, float size) {
    CCNode* node = nullptr;
    if (id == ArrowPaletteID) node = geobr::arrow::portal::createPreview(size);
    else if (objectExists(id)) node = GameObject::createWithKey(id);
    if (!node) node = geobr::ui::makeLabel("?", "bigFont.fnt", .5f);
    auto box = node->getContentSize();
    float extent = std::max({1.f, box.width, box.height});
    node->setScale(std::min(1.2f, size / extent));
    return node;
}

class GodEditorLayer;

// --------------------------------------------------------------- palette
class PalettePopup final : public geode::Popup {
public:
    using Pick = std::function<void(int)>;
    static PalettePopup* create(Pick pick) {
        auto popup = new PalettePopup;
        if (popup->init(std::move(pick))) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

private:
    Pick m_pick;
    CCNode* m_grid = nullptr;
    TextInput* m_input = nullptr;
    int m_tab = 0;

    bool init(Pick pick) {
        if (!Popup::init(440, 270)) return false;
        geobr::visual::fitPopup(m_mainLayer, {440, 270});
        geobr::visual::stylePopup(m_mainLayer, m_bgSprite, {440, 270}, {36, 52, 96});
        setTitle("Objetos", "goldFont.fnt", .7f, 16);
        m_pick = std::move(pick);
        render();
        return true;
    }

    void render() {
        if (m_grid) m_grid->removeFromParent();
        m_grid = CCNode::create();
        m_mainLayer->addChild(m_grid, 5);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_grid->addChild(menu);
        auto const& tabs = paletteTabs();
        int tabCount = static_cast<int>(tabs.size()) + 1;
        for (int i = 0; i < tabCount; ++i) {
            auto name = i < static_cast<int>(tabs.size()) ? tabs[i].name : "ID";
            auto button = geobr::ui::makeTextButton(name, 66, i == m_tab ? "GJ_button_01.png" : "GJ_button_04.png", this, menu_selector(PalettePopup::onTab), .3f, 24);
            button->setTag(i);
            button->setPosition({44.f + i * 70.f, 222});
            menu->addChild(button);
        }
        if (m_tab >= static_cast<int>(tabs.size())) {
            m_input = TextInput::create(160, "ID do objeto", "bigFont.fnt");
            m_input->setCommonFilter(CommonFilter::Uint);
            m_input->setMaxCharCount(4);
            m_input->setPosition({170, 140});
            m_grid->addChild(m_input);
            auto use = geobr::ui::makeTextButton("Usar ID", 110, "GJ_button_01.png", this, menu_selector(PalettePopup::onUseID), .35f, 30);
            use->setPosition({320, 140});
            menu->addChild(use);
            auto hint = geobr::ui::makeLabel("Qualquer objeto do editor pelo ID (1-5000)", "chatFont.fnt", .5f, geobr::ui::palette::TextSoft);
            hint->setPosition({220, 95});
            m_grid->addChild(hint);
            return;
        }
        int slot = 0;
        for (int id : tabs[m_tab].ids) {
            if (!objectExists(id)) continue;
            auto holder = CCNode::create();
            holder->setContentSize({40, 40});
            auto frame = geobr::ui::Frame::create({40, 40}, {28, 34, 62}, 255, false);
            frame->setPosition({20, 20});
            holder->addChild(frame);
            auto preview = objectPreview(id, 28);
            preview->setPosition({20, 20});
            holder->addChild(preview, 2);
            auto button = CCMenuItemSpriteExtra::create(holder, this, menu_selector(PalettePopup::onObject));
            button->setTag(id);
            button->setPosition({52.f + (slot % 8) * 48.f, 178.f - (slot / 8) * 50.f});
            menu->addChild(button);
            ++slot;
        }
    }

    void onTab(CCObject* sender) {
        m_tab = sender->getTag();
        render();
    }

    void onObject(CCObject* sender) {
        if (m_pick) m_pick(sender->getTag());
        onClose(nullptr);
    }

    void onUseID(CCObject*) {
        if (!m_input) return;
        std::string text = m_input->getString();
        int id = 0;
        auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
        if (text.empty() || parsed.ec != std::errc{} || !objectExists(id)) {
            Notification::create("ID de objeto invalido", NotificationIcon::Warning, 1.5f)->show();
            return;
        }
        if (m_pick) m_pick(id);
        onClose(nullptr);
    }
};

// ---------------------------------------------------------------- colours
class ColorPopup final : public geode::Popup {
public:
    using Apply = std::function<void(int, ccColor3B)>;
    static ColorPopup* create(Apply apply) {
        auto popup = new ColorPopup;
        if (popup->init(std::move(apply))) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

private:
    Apply m_apply;
    int m_channel = 1000;
    CCNode* m_view = nullptr;

    static std::vector<std::pair<char const*, int>> const& channels() {
        static std::vector<std::pair<char const*, int>> const list {
            {"Fundo", 1000}, {"Chao", 1001}, {"Chao 2", 1009}, {"Linha", 1002}, {"Objetos", 1004},
            {"Canal 1", 1}, {"Canal 2", 2}, {"Canal 3", 3}, {"Canal 4", 4},
        };
        return list;
    }

    bool init(Apply apply) {
        if (!Popup::init(440, 270)) return false;
        geobr::visual::fitPopup(m_mainLayer, {440, 270});
        geobr::visual::stylePopup(m_mainLayer, m_bgSprite, {440, 270}, {36, 52, 96});
        setTitle("Cores", "goldFont.fnt", .7f, 16);
        m_apply = std::move(apply);
        render();
        return true;
    }

    void render() {
        if (m_view) m_view->removeFromParent();
        m_view = CCNode::create();
        m_mainLayer->addChild(m_view, 5);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_view->addChild(menu);
        auto const& list = channels();
        for (size_t i = 0; i < list.size(); ++i) {
            auto button = geobr::ui::makeTextButton(list[i].first, 88, list[i].second == m_channel ? "GJ_button_01.png" : "GJ_button_04.png", this, menu_selector(ColorPopup::onChannel), .3f, 22);
            button->setTag(list[i].second);
            button->setPosition({58.f + (i % 5) * 81.f, 222.f - (i / 5) * 26.f});
            menu->addChild(button);
        }
        auto gm = GameManager::sharedState();
        for (int i = 0; i < 32; ++i) {
            auto color = gm->colorForIdx(i);
            auto swatch = geobr::ui::Frame::create({34, 26}, color, 255, false);
            auto holder = CCNode::create();
            holder->setContentSize({34, 26});
            swatch->setPosition({17, 13});
            holder->addChild(swatch);
            auto button = CCMenuItemSpriteExtra::create(holder, this, menu_selector(ColorPopup::onSwatch));
            button->setTag(i);
            button->setPosition({52.f + (i % 8) * 48.f, 150.f - (i / 8) * 32.f});
            menu->addChild(button);
        }
    }

    void onChannel(CCObject* sender) {
        m_channel = sender->getTag();
        render();
    }

    void onSwatch(CCObject* sender) {
        if (m_apply) m_apply(m_channel, GameManager::sharedState()->colorForIdx(sender->getTag()));
    }
};

// ---------------------------------------------------------------- overlay
class GodEditorLayer final : public CCLayer {
public:
    static GodEditorLayer* create(PlayLayer* play) {
        auto layer = new GodEditorLayer;
        if (layer->init(play)) {
            layer->autorelease();
            return layer;
        }
        delete layer;
        return nullptr;
    }

private:
    WeakRef<PlayLayer> m_play;
    int m_objectID = 1;
    std::vector<Ref<GameObject>> m_placed;
    struct Pending { Ref<GameObject> object; int frames = 0; int stage = 0; };
    std::vector<Pending> m_pending;   // placed objects not yet drawn by GD
    std::vector<std::pair<int, ccColor3B>> m_colors;
    std::array<bool, 4> m_held {}; // left, right, up, down
    std::array<CCRect, 4> m_padRects {};
    CCTouch* m_padTouch = nullptr;
    float m_zoom = 1.f;
    CCNode* m_selected = nullptr;
    CCLabelBMFont* m_info = nullptr;
    CCMenu* m_menu = nullptr;
    bool m_dirty = false;
    bool m_suspended = false;

    // While GD's pause menu (or any popup) is open the overlay must not
    // swallow touches, or the pause menu could not be used (a "freeze").
    bool blocked() {
        auto play = m_play.lock();
        if (!play || play->m_isPaused) return true;
        auto scene = getParent();
        if (!scene) return true;
        for (auto child : CCArrayExt<CCNode*>(scene->getChildren())) {
            if (typeinfo_cast<FLAlertLayer*>(child) || typeinfo_cast<PauseLayer*>(child)) return true;
        }
        return false;
    }

    bool init(PlayLayer* play) {
        if (!CCLayer::init()) return false;
        m_play = play;
        setID(OverlayID);
        auto win = CCDirector::sharedDirector()->getWinSize();
        m_zoom = std::clamp(play->m_gameState.m_cameraZoom, .4f, 2.f);

        auto bar = geobr::ui::Frame::create({win.width - 20.f, 34.f}, {24, 34, 70}, 220);
        bar->setPosition({win.width / 2, win.height - 22.f});
        addChild(bar);
        auto title = geobr::ui::makeLabel("GOD EDITOR", "goldFont.fnt", .55f);
        title->setAnchorPoint({0, .5f});
        title->setPosition({20.f, win.height - 22.f});
        addChild(title);

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        // RC7 called setHandlerPriority here, before the menu was registered
        // with the touch dispatcher: cocos looks the handler up, gets null and
        // dereferences it (the crash when opening God mode). The priority is
        // now set before onEnter, so the menu registers with it directly.
        menu->setTouchPriority(OverlayMenuPriority);
        addChild(menu, 5);
        m_menu = menu;
        struct Action { char const* label; char const* bg; SEL_MenuHandler handler; };
        Action const actions[] = {
            {"Objetos", "GJ_button_01.png", menu_selector(GodEditorLayer::onObjects)},
            {"Cores", "GJ_button_03.png", menu_selector(GodEditorLayer::onColors)},
            {"Salvar", "GJ_button_05.png", menu_selector(GodEditorLayer::onSave)},
            {"Sair", "GJ_button_06.png", menu_selector(GodEditorLayer::onLeave)},
        };
        for (int i = 0; i < 4; ++i) {
            auto button = geobr::ui::makeTextButton(actions[i].label, 76, actions[i].bg, this, actions[i].handler, .3f, 26);
            button->setPosition({win.width - 52.f - (3 - i) * 82.f, win.height - 22.f});
            menu->addChild(button);
        }
        for (int i = 0; i < 2; ++i) {
            auto button = geobr::ui::makeTextButton(i ? "-" : "+", 36, "GJ_button_04.png", this, menu_selector(GodEditorLayer::onZoom), .5f, 32);
            button->setTag(i ? -1 : 1);
            button->setPosition({win.width - 30.f, win.height / 2 + (i ? -22.f : 22.f)});
            menu->addChild(button);
        }

        // D-pad: hold to fly (platformer-style control in any gamemode).
        CCPoint center {72.f, 72.f};
        CCPoint const offsets[] = {{-40, 0}, {40, 0}, {0, 40}, {0, -40}};
        float const rotations[] = {0.f, 180.f, 90.f, -90.f};
        for (int i = 0; i < 4; ++i) {
            auto pad = geobr::ui::Frame::create({36, 36}, {30, 40, 80}, 200);
            pad->setPosition(center + offsets[i]);
            addChild(pad);
            if (auto arrowSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
                arrowSprite->setScale(.42f);
                arrowSprite->setRotation(rotations[i]);
                arrowSprite->setPosition(center + offsets[i]);
                addChild(arrowSprite, 2);
            }
            auto p = center + offsets[i];
            m_padRects[i] = CCRect(p.x - 22.f, p.y - 22.f, 44.f, 44.f);
        }

        m_info = geobr::ui::makeLabel("", "chatFont.fnt", .55f, geobr::ui::palette::TextGold);
        m_info->setPosition({win.width / 2, 18.f});
        addChild(m_info);
        selectObject(1);

        setTouchEnabled(true);
        scheduleUpdate();
        g_active = play;
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, OverlayPriority, true);
    }

    void onExit() override {
        if (auto play = m_play.lock(); play && g_active == play.data()) g_active = nullptr;
        else if (!m_play.lock()) g_active = nullptr;
        CCLayer::onExit();
    }

    int padAt(CCPoint point) const {
        for (int i = 0; i < 4; ++i) if (m_padRects[i].containsPoint(point)) return i;
        return -1;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (blocked()) return false;
        auto point = touch->getLocation();
        if (int pad = padAt(point); pad >= 0) {
            m_padTouch = touch;
            m_held = {};
            m_held[pad] = true;
            return true;
        }
        place(point);
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (touch != m_padTouch) return;
        m_held = {};
        if (int pad = padAt(touch->getLocation()); pad >= 0) m_held[pad] = true;
    }

    void ccTouchEnded(CCTouch* touch, CCEvent*) override {
        if (touch == m_padTouch) {
            m_padTouch = nullptr;
            m_held = {};
        }
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override { ccTouchEnded(touch, event); }

    void update(float dt) override {
        auto play = m_play.lock();
        if (!play) return;
        bool suspended = blocked();
        if (suspended != m_suspended) {
            m_suspended = suspended;
            setVisible(!suspended);
            if (m_menu) m_menu->setEnabled(!suspended);
            m_held = {};
            m_padTouch = nullptr;
        }
        if (suspended) return;
        dt = std::clamp(dt, 0.f, .1f);
        CCPoint move {
            (m_held[1] ? 1.f : 0.f) - (m_held[0] ? 1.f : 0.f),
            (m_held[2] ? 1.f : 0.f) - (m_held[3] ? 1.f : 0.f),
        };
        for (auto player : {play->m_player1, play->m_player2}) {
            if (!player) continue;
            auto pos = player->getPosition() + move * (FlySpeed * dt);
            pos.y = std::max(pos.y, 105.f);
            player->setPosition(pos);
            player->m_position = pos;
            player->m_yVelocity = 0.0;
        }
        play->m_gameState.m_cameraZoom = m_zoom;
        play->m_gameState.m_targetCameraZoom = m_zoom;
        makePlacedVisible(play.data());
    }

    void makePlacedVisible(PlayLayer* play) {
        for (auto& entry : m_pending) {
            auto object = entry.object.data();
            if (!object || object->m_isActivated) continue;
            if (++entry.frames < 2) continue;
            entry.frames = 0;
            if (entry.stage == 0) { play->addToSection(object); play->sortSectionVector(); }
            else if (entry.stage == 1) object->activateObject();
            ++entry.stage;
        }
        std::erase_if(m_pending, [](Pending const& entry) {
            return !entry.object || entry.object->m_isActivated || entry.stage > 1;
        });
    }

    void selectObject(int id) {
        m_objectID = id;
        if (m_selected) m_selected->removeFromParent();
        auto win = CCDirector::sharedDirector()->getWinSize();
        m_selected = objectPreview(id, 30);
        m_selected->setPosition({win.width - 30.f, 60.f});
        addChild(m_selected, 3);
        refreshInfo();
    }

    void refreshInfo() {
        auto name = m_objectID == ArrowPaletteID ? std::string("Portal Arrow") : fmt::format("Objeto {}", m_objectID);
        m_info->setString(fmt::format("{} | toque para colocar | {} novo(s){}", name, m_placed.size(), m_dirty ? " - nao salvo" : "").c_str());
        m_info->limitLabelWidth(CCDirector::sharedDirector()->getWinSize().width - 180.f, .55f, .3f);
    }

    void place(CCPoint screen) {
        auto play = m_play.lock();
        if (!play || !play->m_objectLayer) return;
        auto local = play->m_objectLayer->convertToNodeSpace(screen);
        // Snap to the editor grid (blocks are 30 units, ground line at 90).
        CCPoint snapped {std::floor(local.x / 30.f) * 30.f + 15.f, std::floor(local.y / 30.f) * 30.f + 15.f};
        if (snapped.y < 105.f) return;
        int id = m_objectID == ArrowPaletteID ? geobr::arrow::portal::BaseObjectID : m_objectID;
        auto object = GameObject::createWithKey(id);
        if (!object) return;
        object->setPosition(snapped);
        object->m_startPosition = snapped;
        if (m_objectID == ArrowPaletteID) geobr::arrow::portal::markAsArrowPortal(object);
        play->addObject(object);
        // RC8 objects only showed after leaving God mode (the level reset
        // rebuilds the sections). GD's visibility pass walks per-section
        // lists that are prepared once at load; sorting them again is the
        // same step the load does. If the object is still not drawn next
        // frame it is added to its section, and as a last resort activated.
        play->sortSectionVector();
        m_pending.push_back({object});
        if (m_objectID == ArrowPaletteID) geobr::arrow::portal::track(play.data(), object);
        m_placed.emplace_back(object);
        m_dirty = true;
        refreshInfo();
    }

    void onObjects(CCObject*) {
        WeakRef<GodEditorLayer> weak = this;
        if (auto popup = PalettePopup::create([weak](int id) { if (auto self = weak.lock()) self->selectObject(id); })) popup->show();
    }

    void onColors(CCObject*) {
        WeakRef<GodEditorLayer> weak = this;
        auto popup = ColorPopup::create([weak](int channel, ccColor3B color) {
            auto self = weak.lock();
            if (!self) return;
            auto play = self->m_play.lock();
            if (!play) return;
            ccHSVValue hsv {0.f, 1.f, 1.f, false, false};
            auto value = color;
            play->updateColor(value, 0.f, channel, false, 1.f, hsv, 0, false, nullptr, 0, 0);
            std::erase_if(self->m_colors, [channel](auto const& entry) { return entry.first == channel; });
            self->m_colors.emplace_back(channel, color);
            self->m_dirty = true;
            self->refreshInfo();
        });
        if (popup) popup->show();
    }

    void onZoom(CCObject* sender) {
        m_zoom = std::clamp(m_zoom + (sender->getTag() > 0 ? .1f : -.1f), .4f, 2.f);
    }

    // Appends the new objects (and colour changes, as start colour triggers)
    // to the level string, exactly where the editor would have put them.
    void onSave(CCObject*) {
        auto play = m_play.lock();
        if (!play || !play->m_level) return;
        if (m_placed.empty() && m_colors.empty()) {
            Notification::create("Nada novo para salvar", NotificationIcon::Info, 1.5f)->show();
            return;
        }
        std::string raw = ZipUtils::decompressString(play->m_level->m_levelString, false, 0);
        if (raw.empty()) {
            Notification::create("Nao foi possivel ler o level", NotificationIcon::Error, 2.f)->show();
            return;
        }
        if (raw.back() != ';') raw += ';';
        for (auto const& object : m_placed) {
            std::string saved = object->getSaveString(play.data());
            if (!saved.empty()) raw += saved + ";";
        }
        for (auto const& [channel, color] : m_colors) {
            raw += fmt::format("1,899,2,0,3,{},7,{},8,{},9,{},10,0,23,{};", 405 + channel % 7 * 30, color.r, color.g, color.b, channel);
        }
        play->m_level->m_levelString = ZipUtils::compressString(raw, false, 0);
        LocalLevelManager::sharedState()->save();
        Notification::create(fmt::format("Salvo: {} objeto(s), {} cor(es)", m_placed.size(), m_colors.size()), NotificationIcon::Success, 2.f)->show();
        m_placed.clear();
        m_colors.clear();
        m_dirty = false;
        refreshInfo();
    }

    void onLeave(CCObject*) {
        if (m_dirty) {
            WeakRef<GodEditorLayer> weak = this;
            createQuickPopup("Sair do God Editor?", "Ha alteracoes nao salvas. Elas continuam nesta tentativa, mas somem ao sair do level.",
                "Voltar", "Sair", [weak](auto, bool leave) { if (auto self = weak.lock(); self && leave) self->close(); });
            return;
        }
        close();
    }

    void close() {
        auto play = m_play.lock();
        g_active = nullptr;
        removeFromParent();
        if (play) {
            play->m_gameState.m_targetCameraZoom = 1.f;
            play->resetLevel();
        }
    }
};

} // namespace

namespace geobr::god {

bool canOpen(PlayLayer* layer) {
    // "Testando sua fase": only your own editor levels can be changed.
    return layer && layer->m_level && layer->m_level->m_levelType == GJLevelType::Editor;
}

void start(PlayLayer* layer) {
    if (!canOpen(layer)) return;
    auto scene = layer->getParent();
    if (!scene || scene->getChildByID(OverlayID)) return;
    if (auto overlay = GodEditorLayer::create(layer)) scene->addChild(overlay, 1000);
}

bool isActive(GJBaseGameLayer* layer) {
    return layer && g_active == layer;
}

bool isActive(PlayerObject* player) {
    return player && g_active && player->m_gameLayer == g_active;
}

} // namespace geobr::god

// --------------------------------------------------------------- hooks
class $modify(GeoBRGodPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto play = PlayLayer::get();
        if (!geobr::god::canOpen(play)) return;
        auto win = CCDirector::sharedDirector()->getWinSize();
        auto icon = CCSprite::create("geobr-god-editor.png"_spr);
        if (!icon) return;
        icon->setScale(44.f / std::max(1.f, icon->getContentWidth()));
        auto button = CCMenuItemSpriteExtra::create(icon, this, menu_selector(GeoBRGodPause::onGodEditor));
        button->setID("geobr-god-editor-button");
        auto menu = CCMenu::create();
        menu->setID("geobr-god-editor-menu");
        menu->setPosition({win.width - 34.f, win.height / 2.f});
        menu->addChild(button);
        auto label = CCLabelBMFont::create("God", "bigFont.fnt");
        label->setScale(.28f);
        label->setPosition({0.f, -28.f});
        menu->addChild(label);
        addChild(menu, 20);
    }

    void onGodEditor(CCObject*) {
        auto play = PlayLayer::get();
        if (!geobr::god::canOpen(play)) return;
        PauseLayer::onResume(nullptr); // removes this pause layer
        geobr::god::start(play);
    }
};

class $modify(GeoBRGodPlayer, PlayerObject) {
    void update(float dt) {
        // Free flight: the God Editor moves the player directly.
        if (geobr::god::isActive(this)) return;
        PlayerObject::update(dt);
    }
};

class $modify(GeoBRGodGameLayer, GJBaseGameLayer) {
    int checkCollisions(PlayerObject* player, float dt, bool ignoreDamage) {
        if (geobr::god::isActive(this)) return 0;
        return GJBaseGameLayer::checkCollisions(player, dt, ignoreDamage);
    }
};

class $modify(GeoBRGodPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (geobr::god::isActive(this)) return;
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        if (geobr::god::isActive(this)) return;
        PlayLayer::levelComplete();
    }

    // Flying past the end must not start GD's end animation (it takes over
    // the player and the camera while the overlay is still active).
    void playEndAnimationToPos(CCPoint position) {
        if (geobr::god::isActive(this)) return;
        PlayLayer::playEndAnimationToPos(position);
    }

    void playPlatformerEndAnimationToPos(CCPoint position, bool instant) {
        if (geobr::god::isActive(this)) return;
        PlayLayer::playPlatformerEndAnimationToPos(position, instant);
    }
};
