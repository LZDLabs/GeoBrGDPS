#include "UIHelpers.hpp"
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <cstring>
using namespace geode::prelude;
namespace geobr::ui {
namespace {

constexpr float Chamfer = 2.5f;   // "almost square" corners
constexpr float Contour = 2.f;    // black outline
constexpr float BorderWidth = 1.f; // thin white border
constexpr CCPoint ShadowOffset {3.f, -3.f};

ccColor4F rgba(ccColor3B c, float a) {
    return {c.r / 255.f, c.g / 255.f, c.b / 255.f, a};
}

ccColor3B lighten(ccColor3B c, float amount) {
    auto up = [amount](GLubyte v) { return static_cast<GLubyte>(std::min(255.f, v + (255.f - v) * amount)); };
    return {up(c.r), up(c.g), up(c.b)};
}

// Octagon with chamfered corners inside the rect (x, y, w, h).
std::array<CCPoint, 8> chamfered(float x, float y, float w, float h, float c) {
    c = std::min({c, w / 2.f, h / 2.f});
    return {{{x + c, y}, {x + w - c, y}, {x + w, y + c}, {x + w, y + h - c},
             {x + w - c, y + h}, {x + c, y + h}, {x, y + h - c}, {x, y + c}}};
}

} // namespace

Frame* Frame::create(CCSize size, ccColor3B fill, GLubyte opacity, bool shadow) {
    auto frame = new Frame;
    if (frame->init(size, fill, opacity, shadow)) {
        frame->autorelease();
        return frame;
    }
    delete frame;
    return nullptr;
}

bool Frame::init(CCSize size, ccColor3B fill, GLubyte opacity, bool shadow) {
    if (!CCNodeRGBA::init()) return false;
    m_shadow = shadow;
    m_draw = CCDrawNode::create();
    addChild(m_draw, -10);
    ignoreAnchorPointForPosition(false);
    setAnchorPoint({.5f, .5f});
    _displayedColor = _realColor = fill;
    _displayedOpacity = _realOpacity = opacity;
    CCNodeRGBA::setContentSize(size);
    redraw();
    return true;
}

void Frame::setContentSize(CCSize const& size) {
    CCNodeRGBA::setContentSize(size);
    if (m_draw) redraw();
}

void Frame::setColor(ccColor3B const& color) {
    CCNodeRGBA::setColor(color);
    redraw();
}

void Frame::setOpacity(GLubyte opacity) {
    CCNodeRGBA::setOpacity(opacity);
    redraw();
}

void Frame::updateDisplayedOpacity(GLubyte parentOpacity) {
    CCNodeRGBA::updateDisplayedOpacity(parentOpacity);
    redraw();
}

void Frame::setBorderColor(ccColor3B color) {
    m_border = color;
    redraw();
}

void Frame::redraw() {
    if (!m_draw) return;
    m_draw->clear();
    auto size = getContentSize();
    if (size.width < 2 || size.height < 2) return;
    float alpha = getDisplayedOpacity() / 255.f;
    auto fill = getColor();
    // CCDrawNode blends premultiplied (GL_ONE, 1 - a): scale colour by alpha.
    auto premul = [](ccColor4F c) { return ccColor4F{c.r * c.a, c.g * c.a, c.b * c.a, c.a}; };
    auto polygon = [&](std::array<CCPoint, 8> points, ccColor4F color) {
        m_draw->drawPolygon(points.data(), static_cast<unsigned>(points.size()), premul(color), 0.f, {0, 0, 0, 0});
    };
    // Outline and border are stroked along the edges only, so translucent
    // panels are not darkened by a solid black polygon underneath (RC6 bug).
    auto stroke = [&](std::array<CCPoint, 8> points, float radius, ccColor4F color) {
        for (size_t i = 0; i < points.size(); ++i) {
            m_draw->drawSegment(points[i], points[(i + 1) % points.size()], radius, premul(color));
        }
    };
    float w = size.width, h = size.height;
    float inset = Contour + BorderWidth;
    if (m_shadow && alpha > .9f) polygon(chamfered(ShadowOffset.x, ShadowOffset.y, w, h, Chamfer), {0, 0, 0, .35f});
    polygon(chamfered(inset, inset, w - inset * 2, h - inset * 2, Chamfer - 1.f), rgba(fill, alpha));
    // GD-like depth: a lighter band over the upper part of the fill.
    float band = std::min(h * .38f, h - inset * 2);
    if (band > 4) polygon(chamfered(inset, h - inset - band, w - inset * 2, band, Chamfer - 1.f), rgba(lighten(fill, .12f), alpha * .55f));
    float half = Contour / 2.f;
    stroke(chamfered(Contour + BorderWidth / 2, Contour + BorderWidth / 2, w - (Contour * 2 + BorderWidth), h - (Contour * 2 + BorderWidth), Chamfer - .5f),
        BorderWidth / 2, rgba(m_border, .9f * std::max(.6f, alpha)));
    stroke(chamfered(half, half, w - Contour, h - Contour, Chamfer), half, {0, 0, 0, std::max(.7f, alpha)});
}

Button* Button::create(CCNode* sprite, CCObject* target, SEL_MenuHandler callback) {
    auto button = new Button;
    if (button->init(sprite, nullptr, target, callback)) {
        button->autorelease();
        return button;
    }
    delete button;
    return nullptr;
}

void Button::setEnabled(bool enabled) {
    CCMenuItemSpriteExtra::setEnabled(enabled);
    if (auto rgba = typeinfo_cast<CCRGBAProtocol*>(getNormalImage())) {
        rgba->setCascadeOpacityEnabled(true);
        rgba->setOpacity(enabled ? 255 : 120);
    }
}

CCLabelBMFont* makeLabel(std::string const& t, char const* f, float s, ccColor3B c) {
    auto l = CCLabelBMFont::create(t.c_str(), f);
    l->setScale(s);
    l->setColor(c);
    return l;
}

Frame* makePanel(CCSize s, ccColor3B c, GLubyte o) {
    return Frame::create(s, c, o, true);
}

ccColor3B buttonColor(char const* bg) {
    std::string name = bg ? bg : "";
    if (name.find("_01") != std::string::npos) return palette::Green;
    if (name.find("_02") != std::string::npos) return palette::Blue;
    if (name.find("_03") != std::string::npos) return palette::Cyan;
    if (name.find("_05") != std::string::npos) return palette::Gold;
    if (name.find("_06") != std::string::npos) return palette::Red;
    return palette::Slate;
}

CCMenuItemSpriteExtra* makeTextButton(std::string const& text, int width, char const* bg, CCObject* target, SEL_MenuHandler cb, float scale, float height) {
    auto node = CCNodeRGBA::create();
    node->setContentSize({float(width), height});
    node->setCascadeOpacityEnabled(true);
    auto frame = Frame::create({float(width), height}, buttonColor(bg), 255, true);
    frame->setPosition({width / 2.f, height / 2.f});
    node->addChild(frame);
    // Pusab (bigFont) text with GD's black shadow, fitted to the button.
    auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    float fit = std::min({scale * 1.45f, (width - 12.f) / std::max(1.f, label->getContentWidth()), (height - 8.f) / std::max(1.f, label->getContentHeight())});
    label->setScale(std::max(.12f, fit));
    label->setPosition({width / 2.f, height / 2.f + 1.f});
    auto shadow = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    shadow->setScale(label->getScale());
    shadow->setColor({0, 0, 0});
    shadow->setOpacity(110);
    shadow->setPosition(label->getPosition() + CCPoint{1.f, -1.5f});
    node->addChild(shadow, 1);
    node->addChild(label, 2);
    return Button::create(node, target, cb);
}

void clearScroll(ScrollLayer* s) {
    s->m_contentLayer->removeAllChildrenWithCleanup(true);
    s->m_contentLayer->setContentSize(s->getContentSize());
}

std::string compactNumber(std::int64_t v) {
    auto a = v < 0 ? -v : v;
    if (a >= 1000000) return fmt::format("{:.1f}M", double(v) / 1000000.0);
    if (a >= 1000) return fmt::format("{:.1f}K", double(v) / 1000.0);
    return fmt::to_string(v);
}

std::string slice(std::string const& value, std::size_t pos, std::size_t count) {
    if (pos >= value.size()) return {};
    return value.substr(pos, count);
}

} // namespace geobr::ui
