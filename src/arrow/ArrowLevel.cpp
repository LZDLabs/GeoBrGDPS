#include "ArrowPlayer.hpp"
#include "ArrowSkins.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/SelectSettingLayer.hpp>
#include <Geode/binding/StartPosObject.hpp>
#include <Geode/modify/CheckpointObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/LevelSettingsLayer.hpp>
#include <Geode/modify/LevelSettingsObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;
using namespace geobr::arrow;

namespace {

// Custom header key. GD keeps the wave start mode (kA2 = 4) as the vanilla
// fallback; GeoBR reads this key to start the level as Arrow.
constexpr char StartKey[] = "kGBA";
constexpr int WaveStartMode = 4;

LevelSettingsObject* effectiveSettings(GJBaseGameLayer* layer) {
    if (layer->m_startPosObject && layer->m_startPosObject->m_startSettings) {
        return layer->m_startPosObject->m_startSettings;
    }
    return layer->m_levelSettings;
}

void applyPlayers(GJBaseGameLayer* layer, Direction direction) {
    for (auto player : {layer->m_player1, layer->m_player2}) {
        if (!player || !player->m_isDart) continue;
        if (player == layer->m_player2 && !layer->m_gameState.m_isDualMode) continue;
        activate(player, false, &direction);
    }
}

} // namespace

class $modify(GeoBRArrowSettingsObject, LevelSettingsObject) {
    struct Fields {
        bool arrowStart = false;
    };

    gd::string getSaveString() {
        auto saved = LevelSettingsObject::getSaveString();
        if (!m_fields->arrowStart) return saved;
        std::string value = saved;
        value += fmt::format(",{},1", StartKey);
        return gd::string(value);
    }

    static LevelSettingsObject* objectFromDict(CCDictionary* dict) {
        auto settings = LevelSettingsObject::objectFromDict(dict);
        if (settings && dict) {
            if (auto value = dict->valueForKey(StartKey); value && value->intValue() == 1) {
                static_cast<GeoBRArrowSettingsObject*>(settings)->m_fields->arrowStart = true;
            }
        }
        return settings;
    }
};

namespace geobr::arrow {

bool startsWithArrow(LevelSettingsObject* settings) {
    return settings && static_cast<GeoBRArrowSettingsObject*>(settings)->m_fields->arrowStart;
}

void setStartsWithArrow(LevelSettingsObject* settings, bool enabled) {
    if (!settings) return;
    static_cast<GeoBRArrowSettingsObject*>(settings)->m_fields->arrowStart = enabled;
    if (enabled) settings->m_startMode = WaveStartMode;
}

void applyStart(GJBaseGameLayer* layer, LevelSettingsObject* settings) {
    if (!layer || !startsWithArrow(settings)) return;
    applyPlayers(layer, Direction::Down);
    // GD may finish switching the start gamemode after this call returns;
    // re-apply once on the next frame (activation is idempotent).
    WeakRef<GJBaseGameLayer> weak = layer;
    queueInMainThread([weak, settings = Ref(settings)] {
        if (auto strong = weak.lock()) {
            if (startsWithArrow(settings)) applyPlayers(strong, Direction::Down);
        }
    });
}

} // namespace geobr::arrow

class $modify(GeoBRArrowStartLayer, GJBaseGameLayer) {
    void setupLevelStart(LevelSettingsObject* settings) {
        GJBaseGameLayer::setupLevelStart(settings);
        applyStart(this, settings);
    }
};

class $modify(GeoBRArrowCheckpoint, CheckpointObject) {
    struct Fields {
        bool stored = false;
        Snapshot player1;
        Snapshot player2;
    };
};

class $modify(GeoBRArrowPlayLayer, PlayLayer) {
    struct Fields {
        bool restoredFromCheckpoint = false;
    };

    CheckpointObject* createCheckpoint() {
        auto checkpoint = PlayLayer::createCheckpoint();
        if (checkpoint) {
            auto& f = *static_cast<GeoBRArrowCheckpoint*>(checkpoint)->m_fields.operator->();
            f.player1 = snapshot(m_player1);
            f.player2 = snapshot(m_player2);
            f.stored = true;
        }
        return checkpoint;
    }

    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::loadFromCheckpoint(checkpoint);
        if (!checkpoint) return;
        auto& f = *static_cast<GeoBRArrowCheckpoint*>(checkpoint)->m_fields.operator->();
        if (!f.stored) return;
        m_fields->restoredFromCheckpoint = true;
        restore(m_player1, f.player1);
        restore(m_player2, f.player2);
        // Re-apply after GD finishes its own respawn bookkeeping this frame.
        WeakRef<PlayLayer> weak = this;
        auto p1 = f.player1, p2 = f.player2;
        queueInMainThread([weak, p1, p2] {
            if (auto layer = weak.lock()) {
                restore(layer->m_player1, p1);
                restore(layer->m_player2, p2);
            }
        });
    }

    void resetLevel() {
        m_fields->restoredFromCheckpoint = false;
        PlayLayer::resetLevel();
        if (!m_fields->restoredFromCheckpoint) applyStart(this, effectiveSettings(this));
    }
};

class $modify(GeoBRArrowEditorPlaytest, LevelEditorLayer) {
    void onPlaytest() {
        LevelEditorLayer::onPlaytest();
        applyStart(this, effectiveSettings(this));
    }
};

class $modify(GeoBRArrowSettingsLayer, LevelSettingsLayer) {
    struct Fields {
        CCMenuItemSpriteExtra* toggle = nullptr;
        CCNode* frame = nullptr;
    };

    bool init(LevelSettingsObject* object, LevelEditorLayer* layer) {
        if (!LevelSettingsLayer::init(object, layer)) return false;
        addArrowToggle();
        return true;
    }

    void addArrowToggle() {
        if (!m_mainLayer || !m_settingsObject) return;
        auto content = CCNode::create();
        content->setContentSize({46.f, 46.f});
        auto frame = CCLayerColor::create({0, 0, 0, 150}, 46.f, 46.f);
        content->addChild(frame);
        auto look = ArrowSettings::localAppearance();
        if (auto icon = ArrowIcon::create(look)) {
            icon->setPosition({23.f, 26.f});
            icon->setRotation(-45.f);
            icon->setScale(.9f);
            content->addChild(icon, 2);
        }
        auto label = CCLabelBMFont::create("ARROW", "bigFont.fnt");
        label->setScale(.26f);
        label->setPosition({23.f, 6.f});
        content->addChild(label, 3);

        auto button = CCMenuItemSpriteExtra::create(content, this, menu_selector(GeoBRArrowSettingsLayer::onArrowStart));
        button->setID("geobr-arrow-start-toggle");
        auto menu = CCMenu::create();
        menu->setID("geobr-arrow-start-menu");
        menu->addChild(button);

        // Next to GD's own start-gamemode button when it can be found.
        CCPoint position {m_mainLayer->getContentSize().width / 2 + 150.f, m_mainLayer->getContentSize().height / 2 - 70.f};
        if (m_modeSprite && m_modeSprite->getParent()) {
            auto world = m_modeSprite->getParent()->convertToWorldSpace(m_modeSprite->getPosition());
            position = m_mainLayer->convertToNodeSpace(world) + CCPoint{m_modeSprite->getScaledContentSize().width / 2 + 34.f, 0.f};
        }
        menu->setPosition(position);
        m_mainLayer->addChild(menu, 20);
        m_fields->toggle = button;
        m_fields->frame = frame;
        refreshArrowToggle();
    }

    void refreshArrowToggle() {
        bool enabled = startsWithArrow(m_settingsObject);
        if (auto frame = typeinfo_cast<CCLayerColor*>(m_fields->frame)) {
            frame->setColor(enabled ? ccColor3B{60, 200, 90} : ccColor3B{0, 0, 0});
            frame->setOpacity(enabled ? 210 : 140);
        }
        if (m_modeSprite) m_modeSprite->setOpacity(enabled ? 90 : 255);
    }

    void onArrowStart(CCObject*) {
        bool enable = !startsWithArrow(m_settingsObject);
        setStartsWithArrow(m_settingsObject, enable);
        refreshArrowToggle();
        if (enable) {
            Notification::create("Inicio no Arrow (portal marrom)", NotificationIcon::Success, 1.5f)->show();
        }
    }

    void selectSettingClosed(SelectSettingLayer* layer) {
        LevelSettingsLayer::selectSettingClosed(layer);
        // Picking a native gamemode replaces the Arrow start.
        if (layer && layer->m_type != SelectSettingType::StartingSpeed && startsWithArrow(m_settingsObject)) {
            setStartsWithArrow(m_settingsObject, false);
            refreshArrowToggle();
        }
    }
};
