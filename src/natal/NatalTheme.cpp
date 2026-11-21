#include "NatalTheme.hpp"
#include "NatalService.hpp"
#include "../models/JsonModelUtils.hpp"

#include <cmath>
#include <random>

using namespace geode::prelude;

namespace geobr::natal::theme {
namespace {

CCSprite* art(char const* file) { return CCSprite::create(Mod::get()->expandSpriteName(file).data()); }

std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}
float rand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng()); }

// Snow in three depth layers: far flakes are small, faint and slow; near ones
// big, bright and fast. Each sways and spins a little; ~60 sprites in total.
class Snow final : public CCNode {
    struct Flake { CCSprite* sprite; float speed, sway, phase, spin; };
    std::vector<Flake> m_flakes;
    float m_time = 0;

public:
    static Snow* create() {
        auto node = new Snow;
        if (node->init()) { node->autorelease(); return node; }
        delete node;
        return nullptr;
    }
    bool init() override {
        if (!CCNode::init()) return false;
        setID("geobr-natal-snow");
        auto win = CCDirector::sharedDirector()->getWinSize();
        struct Layer { int count; float size0, size1, speed0, speed1, alpha0, alpha1; };
        for (auto const& l : {Layer{28, 3.5f, 5.5f, 9.f, 16.f, 90.f, 140.f}, Layer{22, 6.5f, 9.f, 18.f, 28.f, 160.f, 210.f}, Layer{10, 10.f, 14.f, 32.f, 46.f, 215.f, 255.f}}) {
            for (int i = 0; i < l.count; ++i) {
                auto s = art("natal_acc_floco_neve.png");
                if (!s) return true;
                s->setScale(rand(l.size0, l.size1) / std::max(1.f, s->getContentSize().width));
                s->setOpacity(static_cast<GLubyte>(rand(l.alpha0, l.alpha1)));
                s->setPosition({rand(0, win.width), rand(0, win.height)});
                s->setRotation(rand(0, 360));
                addChild(s);
                m_flakes.push_back({s, rand(l.speed0, l.speed1), rand(4, 12), rand(0, 6.28f), rand(-40, 40)});
            }
        }
        scheduleUpdate();
        return true;
    }
    void update(float dt) override {
        m_time += dt;
        auto win = CCDirector::sharedDirector()->getWinSize();
        for (auto& f : m_flakes) {
            auto p = f.sprite->getPosition();
            p.y -= f.speed * dt;
            p.x += std::sin(m_time * .9f + f.phase) * f.sway * dt;
            if (p.y < -12.f) { p.y = win.height + 12.f; p.x = rand(0, win.width); }
            f.sprite->setPosition(p);
            f.sprite->setRotation(f.sprite->getRotation() + f.spin * dt);
        }
    }
};

void sparkle(CCNode* parent, CCPoint at, float delay) {
    auto s = art("natal_spark.png");
    if (!s) return;
    float size = rand(5.f, 9.f);
    float scale = size / std::max(1.f, s->getContentSize().width);
    s->setScale(0);
    s->setPosition(at);
    s->runAction(CCRepeatForever::create(CCSequence::create(
        CCDelayTime::create(delay),
        CCSpawn::create(CCScaleTo::create(.35f, scale), CCRotateBy::create(.7f, 90.f), nullptr),
        CCScaleTo::create(.35f, 0.f),
        CCDelayTime::create(rand(1.2f, 3.f)), nullptr)));
    parent->addChild(s);
}

// A repeating candy-cane strip (the texture is 64x16, power of two, so GL_REPEAT works on phones too).
CCSprite* stripe(float length, float thickness) {
    auto texture = CCTextureCache::sharedTextureCache()->addImage(Mod::get()->expandSpriteName("natal_frame.png").data(), false);
    if (!texture) return nullptr;
    ccTexParams params{GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT};
    texture->setTexParameters(&params);
    auto size = texture->getContentSize();
    float scale = thickness / std::max(1.f, size.height);
    auto s = CCSprite::createWithTexture(texture, CCRect(0, 0, length / scale, size.height));
    if (s) s->setScale(scale);
    return s;
}

CCSprite* decoration(char const* file, float height, CCPoint at, float rotation = 0) {
    auto s = art(file);
    if (!s) return nullptr;
    s->setScale(height / std::max(1.f, s->getContentSize().height));
    s->setPosition(at);
    s->setRotation(rotation);
    return s;
}

} // namespace

bool on() { return natal::decorationsOn(); }

void snow(CCNode* layer, float sparkleLine) {
    if (!layer || !on() || layer->getChildByID("geobr-natal-snow")) return;
    if (auto s = Snow::create()) layer->addChild(s, 100);
    if (sparkleLine < 0) return;
    auto win = CCDirector::sharedDirector()->getWinSize();
    auto sparks = CCNode::create();
    sparks->setID("geobr-natal-sparkles");
    for (int i = 0; i < 12; ++i) sparkle(sparks, {rand(10, win.width - 10), sparkleLine + rand(-3, 4)}, rand(0, 3));
    layer->addChild(sparks, 99);
}

void lights(CCNode* layer) {
    if (!layer || !on() || layer->getChildByID("geobr-natal-lights")) return;
    auto win = CCDirector::sharedDirector()->getWinSize();
    auto garland = CCNode::create();
    garland->setID("geobr-natal-lights");
    float x = 26.f;
    int i = 0;
    while (x < win.width + 20.f) {
        auto s = art("natal_acc_pisca_pisca.png");
        if (!s) break;
        s->setScale(62.f / std::max(1.f, s->getContentSize().width));
        s->setPosition({x, win.height - 10.f});
        s->setFlipY(true);
        s->runAction(CCRepeatForever::create(CCSequence::create(CCDelayTime::create(.3f * (i % 3)), CCFadeTo::create(.6f, 140), CCFadeTo::create(.6f, 255), nullptr)));
        garland->addChild(s);
        x += 58.f;
        ++i;
    }
    layer->addChild(garland, 99);
}

void frame(CCNode* layer, bool corners) {
    if (!layer || !on() || layer->getChildByID("geobr-natal-frame")) return;
    auto win = CCDirector::sharedDirector()->getWinSize();
    auto node = CCNode::create();
    node->setID("geobr-natal-frame");
    constexpr float t = 5.f;
    // The green line of the strip faces the inside on every edge.
    struct Edge { float length; CCPoint at; float rotation; };
    for (auto const& e : {Edge{win.width, {win.width / 2, win.height - t / 2}, 0.f}, Edge{win.width, {win.width / 2, t / 2}, 180.f},
                          Edge{win.height, {t / 2, win.height / 2}, -90.f}, Edge{win.height, {win.width - t / 2, win.height / 2}, 90.f}}) {
        if (auto s = stripe(e.length, t)) { s->setPosition(e.at); s->setRotation(e.rotation); node->addChild(s); }
    }
    if (corners) {
        auto add = [&](char const* file, float h, CCPoint at, float rot) { if (auto s = decoration(file, h, at, rot)) node->addChild(s); };
        add("natal_acc_presente.png", 30.f, {24.f, 22.f}, 0.f);
        add("natal_acc_presente.png", 21.f, {50.f, 17.f}, 8.f);
        add("natal_acc_boneco_gengibre.png", 33.f, {76.f, 24.f}, -8.f);
        add("natal_acc_bengala_doce.png", 38.f, {win.width - 82.f, 26.f}, 12.f);
        add("natal_acc_boneco_neve.png", 40.f, {win.width - 42.f, 27.f}, 0.f);
    }
    layer->addChild(node, 98);
}

void capFlat(CCNode* node, float widthRatio) {
    if (!node || !on() || node->getChildByID("geobr-natal-cap")) return;
    auto size = node->getContentSize();
    auto s = art("natal_snowcap.png");
    if (!s || size.width < 10.f || size.height < 10.f) return;
    auto c = s->getContentSize();  // art: 256x96, body from 26/96 to 54/96 (from the top), icicles below
    float sx = size.width * widthRatio / c.width;
    float sy = sx * .8f;
    s->setID("geobr-natal-cap");
    s->setScaleX(sx);
    s->setScaleY(sy);
    s->setAnchorPoint({.5f, 42.f / 96.f});                      // the base line of the body
    s->setPosition({size.width / 2.f, size.height + 1.f}); // the body sits on top of the edge; only the icicles hang over it
    node->addChild(s, 20);
}

void cap(CCNode* button) {
    if (!button || !on() || button->getChildByID("geobr-natal-cap")) return;
    auto size = button->getContentSize();
    if (size.width < 10.f || size.height < 10.f) return;
    bool round = std::abs(size.width - size.height) < .15f * std::max(size.width, size.height);
    if (!round) { capFlat(button, .96f); return; }
    auto s = art("natal_snowcap_round.png");
    if (!s) return;
    auto c = s->getContentSize();  // art: 256x160, fits a circle centred at (128, 152) with radius 112
    float radius = std::min(size.width, size.height) / 2.f;
    s->setID("geobr-natal-cap");
    s->setScale(radius / (c.width * 112.f / 256.f));
    s->setAnchorPoint({.5f, 1.f - 152.f / 160.f});
    s->setPosition({size.width / 2.f, size.height / 2.f});
    button->addChild(s, 20);
}

void recolorBackground(CCNode* layer, ccColor3B color) {
    if (!layer || !on()) return;
    auto win = CCDirector::sharedDirector()->getWinSize();
    CCSprite* bg = typeinfo_cast<CCSprite*>(layer->getChildByID("background"));
    if (!bg) {
        // Fallback: the first sprite that covers most of the screen.
        for (auto child : CCArrayExt<CCNode*>(layer->getChildren())) {
            auto s = typeinfo_cast<CCSprite*>(child);
            if (!s) continue;
            auto box = s->boundingBox();
            if (box.size.width >= win.width * .8f && box.size.height >= win.height * .8f) { bg = s; break; }
        }
    }
    if (bg) bg->setColor(color);
}

CCNode* gradient(CCSize size) {
    auto g = CCLayerGradient::create({150, 20, 36, 255}, {70, 8, 20, 255});
    g->setContentSize(size);
    g->setID("geobr-natal-gradient");
    return g;
}

void holly(CCNode* card) {
    if (!card || !on() || card->getChildByID("geobr-natal-holly")) return;
    auto size = card->getContentSize();
    if (auto s = decoration("natal_acc_azevinho.png", size.height * .26f, {size.width * .9f, size.height * .92f}, 15.f)) {
        s->setID("geobr-natal-holly");
        card->addChild(s, 20);
    }
}

CCNode* cosmeticIcon(std::string const& id, float size) {
    auto c = natal::cosmetic(id);
    auto kind = c ? jsonutil::stringOr(*c, "kind") : std::string(id.rfind("natal_", 0) == 0 ? "theme" : "frame");
    if (kind == "theme") {
        auto s = CCSprite::create(Mod::get()->expandSpriteName(fmt::format("geobr-theme-{}.png", id)).data());
        if (!s) return nullptr;
        auto rect = s->getTextureRect();
        float side = std::min(rect.size.width, rect.size.height);
        CCRect crop{rect.origin.x + (rect.size.width - side) / 2.f, rect.origin.y, side, side};
        s->setTextureRect(crop, s->isTextureRectRotated(), crop.size);
        s->setScale(size / side);
        return s;
    }
    char const* file = id == "chifres" ? "natal_frame_antlers.png" : id == "neve" ? "natal_snowcap.png" : id == "pisca" ? "natal_acc_pisca_pisca.png"
                     : id == "bengala" ? "natal_acc_bengala_doce.png" : "natal_acc_guirlanda.png";
    auto s = art(file);
    if (!s) return nullptr;
    s->setScale(size / std::max(1.f, std::max(s->getContentWidth(), s->getContentHeight())));
    return s;
}

// Christmas frame drawn over a profile panel (local coordinates of the panel).
void profileFrame(CCNode* panel, std::string const& id) {
    if (!panel) return;
    if (auto old = panel->getChildByID("geobr-natal-frame-deco")) old->removeFromParent();
    if (id.empty()) return;
    auto size = panel->getContentSize();
    auto node = CCNode::create();
    node->setID("geobr-natal-frame-deco");
    node->setContentSize(size);
    float w = size.width, h = size.height;
    auto place = [&](char const* file, float height, CCPoint at, float rotation = 0, bool flipX = false, CCPoint anchor = {.5f, .5f}) -> CCSprite* {
        auto s = art(file);
        if (!s) return nullptr;
        s->setScale(height / std::max(1.f, s->getContentHeight()));
        s->setAnchorPoint(anchor);
        s->setPosition(at);
        s->setRotation(rotation);
        s->setFlipX(flipX);
        node->addChild(s);
        return s;
    };
    if (id == "chifres") {
        // Reindeer antlers standing on the top edge, one on each side.
        place("natal_frame_antlers.png", h * .34f, {w * .24f, h - 6.f}, -8.f, false, {.5f, .02f});
        place("natal_frame_antlers.png", h * .34f, {w * .76f, h - 6.f}, 8.f, true, {.5f, .02f});
    } else if (id == "neve") {
        for (float x = 0; x < w; ) {
            auto s = place("natal_snow_ground.png", 22.f, {x, h + 6.f}, 0, false, {0.f, .5f});
            if (!s) break;
            x += s->getContentWidth() * s->getScale() - 2.f;
        }
        for (float x = 30.f; x < w - 20.f; x += 70.f) place("natal_snowcap.png", 24.f, {x, h - 4.f}, 0, false, {.5f, .6f});
    } else if (id == "pisca") {
        for (float x = 20.f; x < w; x += 56.f) if (auto s = place("natal_acc_pisca_pisca.png", 26.f, {x, h - 2.f})) s->setFlipY(true);
        for (float y = 30.f; y < h - 20.f; y += 56.f) {
            place("natal_acc_pisca_pisca.png", 26.f, {4.f, y}, 90.f);
            place("natal_acc_pisca_pisca.png", 26.f, {w - 4.f, y}, -90.f);
        }
    } else if (id == "bengala") {
        constexpr float t = 9.f;
        struct Edge { float length; CCPoint at; float rotation; };
        for (auto const& e : {Edge{w, {w / 2, h - t / 2}, 0.f}, Edge{w, {w / 2, t / 2}, 180.f}, Edge{h, {t / 2, h / 2}, -90.f}, Edge{h, {w - t / 2, h / 2}, 90.f}}) {
            if (auto s = stripe(e.length, t)) { s->setPosition(e.at); s->setRotation(e.rotation); node->addChild(s); }
        }
        place("natal_acc_bengala_doce.png", 46.f, {16.f, h - 10.f}, -20.f);
        place("natal_acc_bengala_doce.png", 46.f, {w - 16.f, h - 10.f}, 20.f, true);
    } else if (id == "guirlanda") {
        for (float x = 18.f; x < w; x += 34.f) place("natal_acc_azevinho.png", 22.f, {x, h - 2.f}, (int(x) % 3) * 8.f - 8.f);
        place("natal_acc_guirlanda.png", 58.f, {w / 2.f, h + 8.f});
        place("natal_acc_laco_vermelho.png", 30.f, {10.f, h - 6.f}, -15.f);
        place("natal_acc_laco_vermelho.png", 30.f, {w - 10.f, h - 6.f}, 15.f);
    }
    panel->addChild(node, 60);
}

void mainMenu(CCLayer* menu) {
    if (!menu || !on()) return;
    auto win = CCDirector::sharedDirector()->getWinSize();
    constexpr float ground = 90.f; // the menu ground line
    frame(menu, false);
    lights(menu);
    snow(menu, ground + 4.f);
    // Snow on the buttons: the three crosses only on their top arm.
    if (auto main = menu->getChildByID("main-menu")) {
        for (auto id : {"play-button", "icon-kit-button", "editor-button"}) capFlat(main->getChildByID(id), .42f);
    }
    // Snow bank on the ground line.
    if (!menu->getChildByID("geobr-natal-ground")) {
        auto bank = CCNode::create();
        bank->setID("geobr-natal-ground");
        float x = 0.f;
        while (x < win.width) {
            auto s = art("natal_snow_ground.png");
            if (!s) break;
            float scale = 17.f / std::max(1.f, s->getContentSize().height);
            s->setScale(scale);
            s->setAnchorPoint({0.f, 1.f});
            s->setPosition({x - 1.f, ground + 6.f});
            bank->addChild(s);
            x += s->getContentSize().width * scale - 2.f;
        }
        auto bg = menu->getChildByID("main-menu-bg");
        menu->addChild(bank, bg ? bg->getZOrder() : -1);
    }
    // Santa hat on the G of the title, above the lights.
    if (auto title = menu->getChildByID("main-title"); title && !menu->getChildByID("geobr-natal-hat")) {
        auto size = title->getContentSize();
        auto world = title->convertToWorldSpace({size.width * .048f, size.height * .63f});
        float titleScale = title->getScaleY();
        for (auto p = title->getParent(); p && p != menu; p = p->getParent()) titleScale *= p->getScaleY();
        if (auto hat = art("natal_acc_gorro_papai_noel.png")) {
            hat->setID("geobr-natal-hat");
            hat->setAnchorPoint({.5f, .08f});
            hat->setScale(size.height * titleScale * .8f / std::max(1.f, hat->getContentSize().height));
            hat->setRotation(-16.f);
            hat->setPosition(menu->convertToNodeSpace(world));
            menu->addChild(hat, 101);
        }
    }
}

} // namespace geobr::natal::theme
