#include "ArrowPopup.hpp"
#include "ArrowSkins.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/CloudService.hpp"

#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/modify/ProfilePage.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

using namespace geode::prelude;
using namespace geobr::arrow;

namespace {

constexpr char ButtonMenuID[] = "geobr-arrow-profile-menu";

void collectIcons(CCNode* node, std::vector<CCNode*>& out, int depth = 0) {
    if (!node || depth > 7) return;
    for (auto child : CCArrayExt<CCNode*>(node->getChildren())) {
        if (typeinfo_cast<SimplePlayer*>(child)) out.push_back(child);
        else collectIcons(child, out, depth + 1);
    }
}

// The gamemode icon row: the horizontal line with the most SimplePlayers.
std::vector<CCPoint> iconRow(CCNode* root) {
    std::vector<CCNode*> icons;
    collectIcons(root, icons);
    std::map<int, std::vector<CCPoint>> rows;
    for (auto icon : icons) {
        auto parent = icon->getParent();
        if (!parent || !icon->isVisible()) continue;
        auto world = parent->convertToWorldSpace(icon->getPosition());
        rows[static_cast<int>(std::lround(world.y / 6.f))].push_back(world);
    }
    std::vector<CCPoint> best;
    for (auto& [_, points] : rows) if (points.size() > best.size()) best = points;
    std::ranges::sort(best, [](CCPoint a, CCPoint b) { return a.x < b.x; });
    return best.size() >= 3 ? best : std::vector<CCPoint>{};
}

} // namespace

class $modify(GeoBRArrowProfile, ProfilePage) {
    struct Fields {
        geobr::CloudRequest request;
        int remoteSkin = 1;
        bool remoteGlow = false;
    };

    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);
        if (!score) return;
        addArrowButton(score);
        if (!m_ownProfile && score->m_accountID > 0) {
            m_fields->request.rpc("geobr_native_profile", matjson::makeObject({{"p_account", score->m_accountID}}),
                [this](geobr::ApiJsonResult result) {
                    if (!result.success || !result.value.isObject()) return;
                    m_fields->remoteSkin = clampSkin(static_cast<int>(geobr::jsonutil::integerOr(result.value, "arrow_skin", 1)));
                    m_fields->remoteGlow = geobr::jsonutil::boolOr(result.value, "arrow_glow", false);
                });
        }
    }

    Appearance profileAppearance() {
        if (m_ownProfile || !m_score) return ArrowSettings::localAppearance();
        auto gm = GameManager::sharedState();
        Appearance look;
        look.skin = m_fields->remoteSkin;
        look.primary = gm->colorForIdx(m_score->m_color1);
        look.secondary = gm->colorForIdx(m_score->m_color2);
        look.glow = gm->colorForIdx(m_score->m_color3);
        look.glowEnabled = m_score->m_glowEnabled || m_fields->remoteGlow;
        return look;
    }

    void addArrowButton(GJUserScore* score) {
        if (!m_mainLayer) return;
        if (auto old = m_mainLayer->getChildByID(ButtonMenuID)) old->removeFromParent();

        auto icon = ArrowIcon::create(profileAppearance());
        if (!icon) return;
        icon->setScale(.95f);
        auto holder = CCNode::create();
        holder->setContentSize({34.f, 34.f});
        icon->setPosition({17.f, 17.f});
        holder->addChild(icon);
        auto button = CCMenuItemSpriteExtra::create(holder, this, menu_selector(GeoBRArrowProfile::onArrow));
        button->setID("geobr-arrow-profile-button");

        auto menu = CCMenu::create();
        menu->setID(ButtonMenuID);
        menu->addChild(button);

        auto row = iconRow(m_mainLayer);
        CCPoint world;
        if (!row.empty()) {
            float spacing = row.size() > 1 ? (row.back().x - row.front().x) / (row.size() - 1) : 32.f;
            world = row.back() + CCPoint{std::clamp(spacing, 24.f, 44.f), 0.f};
        }
        else if (m_usernameLabel && m_usernameLabel->getParent()) {
            world = m_usernameLabel->getParent()->convertToWorldSpace(m_usernameLabel->getPosition()) + CCPoint{0.f, -62.f};
        }
        else {
            world = CCDirector::sharedDirector()->getWinSize() / 2;
        }
        menu->setPosition(m_mainLayer->convertToNodeSpace(world));
        m_mainLayer->addChild(menu, 30);
        // Subtle bob so the new gamemode is discoverable without a label.
        icon->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCRotateTo::create(.9f, -45.f)),
            CCEaseSineInOut::create(CCRotateTo::create(.9f, 45.f)), nullptr
        )));
        (void)score;
    }

    void onArrow(CCObject*) {
        geobr::arrow::ArrowPopup* popup = nullptr;
        if (m_ownProfile) popup = ArrowPopup::createLocal();
        else {
            std::string name = m_score ? std::string(m_score->m_userName) : "Jogador";
            popup = ArrowPopup::createRemote(name, profileAppearance());
        }
        if (popup) popup->show();
    }
};
