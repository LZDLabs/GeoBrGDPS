#include "ArrowPortal.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CreateMenuItem.hpp>
#include <Geode/binding/EditButtonBar.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_set>
#include <vector>

using namespace geode::prelude;

namespace geobr::arrow::portal {
namespace {

constexpr float PortalHeight = 86.f;  // GD gamemode portal height in points
constexpr char OverlayID[] = "geobr-arrow-portal-overlays";

// Objects confirmed as Arrow portals for the current layer. Pointers are
// only compared, never dereferenced, so stale entries are harmless.
std::unordered_set<GameObject*> g_known;

bool markerMatches(GameObject* object) {
    return object->m_objectID == BaseObjectID && object->m_isHide && object->m_zOrder == MarkerZOrder;
}

CCSprite* portalSprite(char const* part) {
    auto sprite = CCSprite::create(Mod::get()->expandSpriteName(fmt::format("geobr-arrow-portal-{}.png", part)).c_str());
    if (sprite) sprite->setScale(PortalHeight / std::max(1.f, sprite->getContentHeight()));
    return sprite;
}

class OverlayLayer final : public CCNode {
public:
    static OverlayLayer* create(GJBaseGameLayer* layer, bool rescan) {
        auto node = new OverlayLayer;
        if (node->init(layer, rescan)) {
            node->autorelease();
            return node;
        }
        delete node;
        return nullptr;
    }

    void track(GameObject* object) {
        if (!object || m_tracked.contains(object)) return;
        g_known.insert(object);
        Entry entry;
        entry.object = object;
        entry.back = portalSprite("back");
        entry.front = portalSprite("front");
        if (!entry.back || !entry.front) return;
        if (auto pulse = portalSprite("back")) {
            // Soft amber pulse behind the ring: the portal's own animation.
            pulse->setColor({255, 190, 110});
            pulse->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            pulse->setScale(1.f);
            pulse->setPosition(entry.back->getContentSize() / 2);
            pulse->setOpacity(70);
            pulse->runAction(CCRepeatForever::create(CCSequence::create(
                CCFadeTo::create(.7f, 150), CCFadeTo::create(.7f, 50), nullptr
            )));
            entry.back->addChild(pulse, -1);
        }
        attach(entry);
        m_tracked.insert(object);
        m_entries.push_back(std::move(entry));
        sync(0.f);
    }

    void scan() {
        if (!m_layer || !m_layer->m_objects) return;
        std::set<GameObject*> present;
        for (auto object : CCArrayExt<GameObject*>(m_layer->m_objects)) {
            if (!object || object->m_objectID != BaseObjectID) continue;
            if (!markerMatches(object) && !g_known.contains(object)) continue;
            present.insert(object);
            track(object);
        }
        // Removed (deleted / un-marked) portals lose their overlay.
        std::erase_if(m_entries, [&](Entry& entry) {
            bool keep = present.contains(entry.object.data()) && isArrowPortal(entry.object);
            if (!keep) {
                m_tracked.erase(entry.object.data());
                if (entry.back) entry.back->removeFromParent();
                if (entry.front) entry.front->removeFromParent();
            }
            return !keep;
        });
    }

private:
    struct Entry {
        Ref<GameObject> object;
        Ref<CCSprite> back;
        Ref<CCSprite> front;
    };

    GJBaseGameLayer* m_layer = nullptr;
    std::vector<Entry> m_entries;
    std::unordered_set<GameObject*> m_tracked;
    bool m_rescan = false;
    float m_rescanTimer = 0.f;

    bool init(GJBaseGameLayer* layer, bool rescan) {
        if (!CCNode::init()) return false;
        m_layer = layer;
        m_rescan = rescan;
        setID(OverlayID);
        scheduleUpdate();
        return true;
    }

    // Editor: always on top (preview). Gameplay: back half behind the player,
    // front half in front of it, so the Arrow visibly passes through.
    void attach(Entry& entry) {
        auto player = m_layer->m_player1;
        CCNode* parent = m_layer->m_objectLayer;
        int backZ = 9998, frontZ = 9999;
        if (!m_layer->m_isEditor && player && player->getParent()) {
            parent = player->getParent();
            backZ = player->getZOrder() - 1;
            frontZ = player->getZOrder() + 1;
        }
        if (!parent) parent = this;
        parent->addChild(entry.back, backZ);
        parent->addChild(entry.front, frontZ);
    }

    void update(float dt) override {
        if (m_rescan) {
            m_rescanTimer += dt;
            if (m_rescanTimer >= 1.f) {
                m_rescanTimer = 0.f;
                scan();
            }
        }
        sync(dt);
    }

    void sync(float) {
        for (auto& entry : m_entries) {
            auto object = entry.object.data();
            auto reference = object->getParent() ? object->getParent() : static_cast<CCNode*>(m_layer->m_objectLayer);
            if (!reference) continue;
            auto world = reference->convertToWorldSpace(object->getPosition());
            bool visible = !object->m_isGroupDisabled;
            auto color = object->m_isSelected ? ccColor3B{0, 255, 0} : ccColor3B{255, 255, 255};
            for (auto sprite : {entry.back.data(), entry.front.data()}) {
                auto parent = sprite->getParent();
                if (!parent) continue;
                sprite->setPosition(parent->convertToNodeSpace(world));
                sprite->setRotation(object->getRotation());
                float base = PortalHeight / std::max(1.f, sprite->getContentHeight());
                sprite->setScaleX(base * object->getScaleX());
                sprite->setScaleY(base * object->getScaleY());
                sprite->setFlipX(object->isFlipX());
                sprite->setFlipY(object->isFlipY());
                sprite->setVisible(visible);
                sprite->setColor(color);
            }
        }
    }

    void onExit() override {
        for (auto& entry : m_entries) {
            if (entry.back) entry.back->removeFromParent();
            if (entry.front) entry.front->removeFromParent();
        }
        m_entries.clear();
        m_tracked.clear();
        CCNode::onExit();
    }
};

OverlayLayer* overlaysOf(GJBaseGameLayer* layer) {
    if (!layer || !layer->m_objectLayer) return nullptr;
    return typeinfo_cast<OverlayLayer*>(layer->m_objectLayer->getChildByID(OverlayID));
}

} // namespace

bool isArrowPortal(GameObject* object) {
    if (!object || object->m_objectID != BaseObjectID) return false;
    return markerMatches(object) || g_known.contains(object);
}

bool isGamemodePortal(GameObject* object) {
    if (!object) return false;
    switch (object->m_objectType) {
        case GameObjectType::CubePortal:
        case GameObjectType::ShipPortal:
        case GameObjectType::BallPortal:
        case GameObjectType::UfoPortal:
        case GameObjectType::WavePortal:
        case GameObjectType::RobotPortal:
        case GameObjectType::SpiderPortal:
        case GameObjectType::SwingPortal:
            return true;
        default:
            return false;
    }
}

void markAsArrowPortal(GameObject* object) {
    if (!object || object->m_objectID != BaseObjectID) return;
    object->m_isHide = true;
    object->m_zOrder = MarkerZOrder;
    g_known.insert(object);
}

void attachOverlays(GJBaseGameLayer* layer, bool rescanPeriodically) {
    if (!layer || !layer->m_objectLayer) return;
    if (auto existing = overlaysOf(layer)) {
        existing->scan();
        return;
    }
    g_known.clear();
    auto overlays = OverlayLayer::create(layer, rescanPeriodically);
    if (!overlays) return;
    layer->m_objectLayer->addChild(overlays);
    overlays->scan();
}

void track(GJBaseGameLayer* layer, GameObject* object) {
    if (auto overlays = overlaysOf(layer)) overlays->track(object);
}

CCNode* createPreview(float height) {
    auto node = CCNode::create();
    node->setContentSize({height * .42f, height});
    node->setAnchorPoint({.5f, .5f});
    for (auto part : {"back", "front"}) {
        if (auto sprite = portalSprite(part)) {
            sprite->setScale(height / std::max(1.f, sprite->getContentHeight()));
            sprite->setPosition(node->getContentSize() / 2);
            node->addChild(sprite);
        }
    }
    return node;
}

} // namespace geobr::arrow::portal

namespace {
using namespace geobr::arrow;

// Positions of a bar's first page, used to detect GD's row/column order.
std::pair<int, int> measureGrid(EditButtonBar* bar) {
    std::set<int> xs, ys;
    CCNode* page = nullptr;
    for (auto item : CCArrayExt<CCNode*>(bar->m_buttonArray)) {
        if (!item || !item->getParent()) continue;
        if (!page) page = item->getParent();
        if (item->getParent() != page) break;
        xs.insert(static_cast<int>(std::lround(item->getPositionX())));
        ys.insert(static_cast<int>(std::lround(item->getPositionY())));
    }
    return {static_cast<int>(xs.size()), static_cast<int>(ys.size())};
}

} // namespace

class $modify(GeoBRArrowEditorUI, EditorUI) {
    struct Fields {
        CreateMenuItem* arrowButton = nullptr;
        bool arrowBrush = false;
    };

    void setupCreateMenu() {
        EditorUI::setupCreateMenu();
        addArrowPortalButton();
    }

    void addArrowPortalButton() {
        if (!m_createButtonBars) return;
        // setupCreateMenu can rebuild the bars (e.g. after changing the
        // buttons-per-row option), so look for our button in the bar itself.
        for (auto bar : CCArrayExt<EditButtonBar*>(m_createButtonBars)) {
            if (!bar || !bar->m_buttonArray) continue;
            int index = -1, i = 0;
            bool present = false;
            for (auto item : CCArrayExt<CCObject*>(bar->m_buttonArray)) {
                auto create = typeinfo_cast<CreateMenuItem*>(item);
                if (create && create->getID() == "geobr-arrow-portal-button") present = true;
                if (create && index < 0 && create->m_objectID == portal::BaseObjectID) index = i;
                ++i;
            }
            if (index < 0) continue;
            if (present) return;

            auto button = getCreateBtn(portal::BaseObjectID, 4);
            if (!button) return;
            button->setID("geobr-arrow-portal-button");
            if (auto sprite = typeinfo_cast<ButtonSprite*>(button->getNormalImage())) {
                if (sprite->m_subSprite) {
                    auto size = sprite->m_subSprite->getScaledContentSize();
                    sprite->m_subSprite->setVisible(false);
                    auto preview = portal::createPreview(std::max(18.f, size.height));
                    preview->setPosition(sprite->m_subSprite->getPosition());
                    sprite->addChild(preview, 5);
                }
            }

            auto [columns, rows] = measureGrid(bar);
            bar->m_buttonArray->insertObject(button, index + 1);
            if (m_createButtonArray) m_createButtonArray->addObject(button);
            auto gm = GameManager::sharedState();
            if (columns <= 0) columns = std::max(1, gm->getIntGameVariable("0049"));
            if (rows <= 0) rows = std::max(1, gm->getIntGameVariable("0050"));
            bar->loadFromItems(bar->m_buttonArray, columns, rows, true);
            // The binding's parameter names are not authoritative: verify the
            // resulting grid and swap once if GD laid it out transposed.
            if (auto [c, r] = measureGrid(bar); c != columns && c == rows) {
                bar->loadFromItems(bar->m_buttonArray, rows, columns, true);
            }
            m_fields->arrowButton = button;
            log::info("GeoBR Arrow: portal marrom adicionado a aba de portais");
            return;
        }
        log::warn("GeoBR Arrow: aba de portais nao encontrada; botao do portal nao adicionado");
    }

    void onCreateButton(CCObject* sender) {
        m_fields->arrowBrush = sender && sender == m_fields->arrowButton;
        EditorUI::onCreateButton(sender);
    }

    GameObject* createObject(int objectID, CCPoint position) {
        auto object = EditorUI::createObject(objectID, position);
        if (object && objectID == portal::BaseObjectID && m_fields->arrowBrush &&
            m_selectedObjectIndex == portal::BaseObjectID) {
            portal::markAsArrowPortal(object);
            portal::track(m_editorLayer, object);
        }
        return object;
    }
};

class $modify(GeoBRArrowEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI)) return false;
        portal::attachOverlays(this, true);
        return true;
    }
};

class $modify(GeoBRArrowPortalPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        portal::attachOverlays(this, false);
    }
};
