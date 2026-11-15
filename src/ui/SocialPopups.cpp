#include "SocialPopups.hpp"
#include "DirectMessagesPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../arrow/ArrowSkins.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../services/AccessService.hpp"
#include "../utils/TextSanitizer.hpp"

#include <Geode/binding/GameManager.hpp>
#include <Geode/ui/TextArea.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace geobr {
namespace {

void label(CCNode* parent, std::string const& text, CCPoint pos, float scale, float width, ccColor3B color = {255, 255, 255}, bool left = false, char const* font = "bigFont.fnt") {
    auto l = ui::makeLabel(text, font, scale, color);
    l->setAnchorPoint({left ? 0.f : .5f, .5f});
    l->setPosition(pos);
    l->limitLabelWidth(width, scale, .18f);
    parent->addChild(l);
}

void setStatus(CCLabelBMFont* status, std::string const& text, float width = 440) {
    if (!status) return;
    status->setString(text.c_str());
    status->limitLabelWidth(width, .38f, .22f);
}

int levelOf(matjson::Value const& player) {
    auto xp = std::max<int64_t>(0, jsonutil::integerOr(player, "lifetime_xp"));
    return static_cast<int>(std::floor(std::sqrt(double(xp) / 100))) + 1;
}

matjson::Value arrayOrEmpty(matjson::Value const& value) {
    return value.isArray() ? value : matjson::Value::array();
}

// A member/player row: avatar, name, a subtitle; the whole row is a button.
CCMenuItemSpriteExtra* playerRow(matjson::Value const& p, float width, std::string const& subtitle, CCObject* target, SEL_MenuHandler handler) {
    auto row = CCNode::create();
    row->setContentSize({width, 44});
    auto panel = ui::makePanel({width, 44}, {22, 32, 62}, 245);
    panel->setPosition({width / 2, 22});
    row->addChild(panel);
    auto icon = visual::avatar(p, 30);
    icon->setPosition({24, 22});
    row->addChild(icon, 2);
    label(row, jsonutil::stringOr(p, "display_name", "Jogador GeoBR"), {46, 29}, .36f, width - 140, {255, 255, 255}, true);
    label(row, subtitle, {46, 12}, .26f, width - 140, {170, 205, 240}, true);
    auto button = CCMenuItemSpriteExtra::create(row, target, handler);
    button->setID(jsonutil::stringOr(p, "user_id"));
    return button;
}

} // namespace

// ------------------------------------------------------------------ chat
ChatPopup* ChatPopup::create(ChatKind kind, std::string id, std::string title) {
    auto p = new ChatPopup;
    if (p->init(kind, std::move(id), std::move(title))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool ChatPopup::init(ChatKind kind, std::string id, std::string title) {
    if (!Popup::init(470, 300)) return false;
    visual::fitPopup(m_mainLayer, {470, 300});
    visual::stylePopup(m_mainLayer, m_bgSprite, {470, 300}, {31, 30, 55});
    setTitle(title.empty() ? "Chat" : title, "goldFont.fnt", .65f, 16);
    setID("geobr-chat");
    m_kind = kind;
    m_id = std::move(id);
    auto panel = ui::makePanel({440, 180}, {14, 24, 43}, 255);
    panel->setPosition({235, 158});
    m_mainLayer->addChild(panel);
    m_list = ScrollLayer::create({432, 174}, true, true);
    m_list->setPosition({19, 71});
    m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(5));
    m_mainLayer->addChild(m_list);
    m_input = TextInput::create(320, "Escreva uma mensagem...", "chatFont.fnt");
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setMaxCharCount(kind == ChatKind::Group ? 300 : 200);
    m_input->setPosition({182, 38});
    m_mainLayer->addChild(m_input);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu);
    m_submit = ui::makeTextButton("Enviar", 100, "GJ_button_01.png", this, menu_selector(ChatPopup::onSend), .33f, 30);
    m_submit->setPosition({400, 38});
    menu->addChild(m_submit);
    m_status = ui::makeLabel("Carregando...", "chatFont.fnt", .38f, {174, 209, 237});
    m_status->setPosition({235, 13});
    m_mainLayer->addChild(m_status);
    fetch();
    schedule(schedule_selector(ChatPopup::fetch), 4);
    return true;
}

void ChatPopup::fetch(float) {
    if (m_fetchPending) return;
    m_fetchPending = true;
    bool group = m_kind == ChatKind::Group;
    m_fetch.rpc(group ? "geobr_group_messages" : "geobr_collab_messages", matjson::makeObject({{group ? "p_group" : "p_room", m_id}}), [this](auto r) {
        m_fetchPending = false;
        if (!r.success) { setStatus(m_status, r.error); return; }
        if (r.value == m_rows) return;
        m_rows = r.value;
        render();
    });
}

void ChatPopup::render() {
    ui::clearScroll(m_list);
    auto rows = arrayOrEmpty(m_rows).asArray().unwrap();
    if (rows.empty()) {
        auto hint = ui::makeLabel("Nenhuma mensagem ainda. Diga oi!", "bigFont.fnt", .36f, {174, 203, 233});
        hint->setPosition({216, 87});
        m_list->m_contentLayer->addChild(hint);
        setStatus(m_status, "As mensagens aparecem aqui para todo mundo da sala.");
        return;
    }
    for (auto const& row : rows) {
        bool mine = jsonutil::stringOr(row, "user_id") == CloudService::userID();
        auto body = jsonutil::stringOr(row, "body");
        float height = body.size() > 70 ? 70.f : 52.f;
        auto node = CCNode::create();
        node->setContentSize({424, height});
        node->setAnchorPoint({.5f, .5f});
        auto bubble = ui::makePanel({390, height}, mine ? ccColor3B{49, 42, 85} : ccColor3B{25, 43, 65}, 255);
        bubble->setPosition({mine ? 229.f : 195.f, height / 2});
        node->addChild(bubble);
        auto avatar = visual::avatar(row, 22);
        avatar->setPosition({20, height - 18});
        bubble->addChild(avatar);
        label(bubble, mine ? std::string("Voce") : jsonutil::stringOr(row, "sender_name", "Jogador"), {38, height - 12}, .28f, 250, {255, 222, 134}, true);
        auto stamp = jsonutil::stringOr(row, "created_at");
        label(bubble, stamp.size() > 15 ? stamp.substr(11, 5) : "", {360, height - 12}, .3f, 50, {155, 189, 216}, false, "chatFont.fnt");
        auto text = SimpleTextArea::create(body, "chatFont.fnt", .48f, 340);
        text->setAnchorPoint({0, 1});
        text->setAlignment(kCCTextAlignmentLeft);
        text->setWrappingMode(WrappingMode::WORD_WRAP);
        text->setMaxLines(3);
        text->setPosition({38, height - 22});
        bubble->addChild(text);
        m_list->m_contentLayer->addChild(node);
    }
    m_list->m_contentLayer->updateLayout();
    m_list->scrollToTop();
    setStatus(m_status, "Atualiza sozinho a cada 4 segundos");
}

void ChatPopup::onSend(CCObject*) {
    if (m_busy) return;
    if (!AccessService::get()->canChat()) { setStatus(m_status, AccessService::get()->reason()); return; }
    auto body = text::sanitizeMessage(m_input->getString().c_str(), m_kind == ChatKind::Group ? 300 : 200);
    if (body.empty()) return;
    if (m_requestID.empty() || m_pendingBody != body) { m_requestID = visual::uuid(); m_pendingBody = body; }
    m_busy = true;
    m_submit->setEnabled(false);
    setStatus(m_status, "Enviando...");
    bool group = m_kind == ChatKind::Group;
    auto args = matjson::makeObject({{group ? "p_group" : "p_room", m_id}, {"p_body", body}, {"p_id", m_requestID}});
    m_send.rpc(group ? "geobr_group_send" : "geobr_collab_send", args, [this](auto r) {
        m_busy = false;
        m_submit->setEnabled(true);
        if (!r.success) {
            if (r.httpStatus >= 400 && r.httpStatus < 500 && r.httpStatus != 401 && r.httpStatus != 429) { m_requestID.clear(); m_pendingBody.clear(); }
            setStatus(m_status, r.error);
            return;
        }
        m_requestID.clear();
        m_pendingBody.clear();
        m_input->setString("");
        setStatus(m_status, "Mensagem enviada.");
        fetch();
    });
}

// ---------------------------------------------------------------- members
MembersPopup* MembersPopup::create(ChatKind kind, std::string id, std::string title) {
    auto p = new MembersPopup;
    if (p->init(kind, std::move(id), std::move(title))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool MembersPopup::init(ChatKind kind, std::string id, std::string title) {
    if (!Popup::init(420, 290)) return false;
    visual::fitPopup(m_mainLayer, {420, 290});
    visual::stylePopup(m_mainLayer, m_bgSprite, {420, 290}, {36, 52, 96});
    setTitle(title.empty() ? "Membros" : title, "goldFont.fnt", .62f, 16);
    setID("geobr-members");
    m_kind = kind;
    m_id = std::move(id);
    m_status = ui::makeLabel("Carregando...", "chatFont.fnt", .38f, {174, 209, 237});
    m_status->setPosition({210, 14});
    m_mainLayer->addChild(m_status, 5);
    fetch();
    return true;
}

void MembersPopup::fetch() {
    bool group = m_kind == ChatKind::Group;
    m_fetch.rpc(group ? "geobr_group_members" : "geobr_collab_members", matjson::makeObject({{group ? "p_group" : "p_room", m_id}}), [this](auto r) {
        if (!r.success) { setStatus(m_status, r.error, 390); return; }
        m_rows = r.value;
        m_owner = false;
        for (auto const& m : arrayOrEmpty(m_rows).asArray().unwrap()) {
            if (jsonutil::stringOr(m, "user_id") == CloudService::userID() && jsonutil::stringOr(m, "role") == "owner") m_owner = true;
        }
        render();
    });
}

void MembersPopup::render() {
    if (m_view) m_view->removeFromParent();
    m_view = CCNode::create();
    m_mainLayer->addChild(m_view, 3);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_view->addChild(menu);
    auto rows = arrayOrEmpty(m_rows).asArray().unwrap();
    int pages = std::max<int>(1, (static_cast<int>(rows.size()) + 3) / 4);
    m_page = std::clamp(m_page, 0, pages - 1);
    bool group = m_kind == ChatKind::Group;
    for (int i = 0; i < 4; ++i) {
        size_t index = m_page * 4 + i;
        if (index >= rows.size()) break;
        auto const& m = rows[index];
        auto role = jsonutil::stringOr(m, "role");
        bool admin = jsonutil::boolOr(m, "can_save", false);
        auto subtitle = role == "owner" ? std::string("Host") : role == "viewer" ? std::string("So ver") : group ? std::string("Membro") : admin ? std::string("ADM (salva)") : std::string("Editor");
        auto row = playerRow(m, 300, fmt::format("{}  |  Nv. {}", subtitle, levelOf(m)), this, menu_selector(MembersPopup::onPlayer));
        row->setTag(static_cast<int>(index));
        row->setPosition({m_owner ? 180.f : 210.f, 225.f - i * 50});
        menu->addChild(row);
        // Collab: the host chooses the ADMs (who can save and publish the level).
        if (!group && m_owner && role != "owner" && role != "viewer") {
            auto toggle = ui::makeTextButton(admin ? "Tirar ADM" : "Dar ADM", 78, admin ? "GJ_button_06.png" : "GJ_button_01.png", this, menu_selector(MembersPopup::onToggleAdmin), .24f, 24);
            toggle->setTag(static_cast<int>(index));
            toggle->setPosition({370, 225.f - i * 50});
            menu->addChild(toggle);
        }
        if (group && m_owner && role != "owner") {
            auto remove = ui::makeTextButton("Remover", 78, "GJ_button_06.png", this, menu_selector(MembersPopup::onRemove), .24f, 24);
            remove->setTag(static_cast<int>(index));
            remove->setPosition({370, 225.f - i * 50});
            menu->addChild(remove);
        }
    }
    if (pages > 1) {
        for (int side = 0; side < 2; ++side) {
            auto b = ui::makeTextButton(side ? ">" : "<", 40, "GJ_button_04.png", this, menu_selector(MembersPopup::onPage), .35f, 24);
            b->setTag(side ? 1 : -1);
            b->setPosition({side ? 380.f : 40.f, 40});
            b->setEnabled(side ? m_page + 1 < pages : m_page > 0);
            menu->addChild(b);
        }
    }
    if (group) {
        auto add = ui::makeTextButton("Adicionar jogador", 170, "GJ_button_01.png", this, menu_selector(MembersPopup::onAdd), .3f, 28);
        add->setPosition({210, 40});
        menu->addChild(add);
    }
    setStatus(m_status, fmt::format("{} membro{}", rows.size(), rows.size() == 1 ? "" : "s"), 390);
}

void MembersPopup::onPage(CCObject* s) { m_page += s->getTag(); render(); }

void MembersPopup::onToggleAdmin(CCObject* s) {
    auto rows = arrayOrEmpty(m_rows).asArray().unwrap();
    auto index = static_cast<size_t>(s->getTag());
    if (index >= rows.size()) return;
    bool admin = jsonutil::boolOr(rows[index], "can_save", false);
    WeakRef<MembersPopup> weak = this;
    m_action.rpc("geobr_collab_set_saver", matjson::makeObject({{"p_room", m_id}, {"p_user", jsonutil::stringOr(rows[index], "user_id")}, {"p_on", !admin}}),
        [weak, admin](auto r) {
            auto self = weak.lock();
            if (!self) return;
            Notification::create(r.success ? (admin ? "ADM removido" : "Agora e ADM: pode salvar e publicar") : r.error,
                r.success ? NotificationIcon::Success : NotificationIcon::Warning, 2.f)->show();
            if (r.success) self->fetch();
        });
}

void MembersPopup::onPlayer(CCObject* s) {
    auto rows = arrayOrEmpty(m_rows).asArray().unwrap();
    size_t index = static_cast<size_t>(s->getTag());
    if (index < rows.size()) if (auto p = PlayerCardPopup::create(rows[index])) p->show();
}

void MembersPopup::onRemove(CCObject* s) {
    auto rows = arrayOrEmpty(m_rows).asArray().unwrap();
    size_t index = static_cast<size_t>(s->getTag());
    if (index >= rows.size()) return;
    auto user = jsonutil::stringOr(rows[index], "user_id");
    m_action.rpc("geobr_group_remove", matjson::makeObject({{"p_group", m_id}, {"p_user", user}}), [this](auto r) {
        setStatus(m_status, r.success ? "Membro removido." : r.error, 390);
        if (r.success) fetch();
    });
}

void MembersPopup::onAdd(CCObject*) {
    WeakRef<MembersPopup> weak = this;
    auto picker = PlayerPickerPopup::create("Adicionar ao grupo", [weak](matjson::Value player) {
        auto self = weak.lock();
        if (!self) return;
        self->m_action.rpc("geobr_group_add", matjson::makeObject({{"p_group", self->m_id}, {"p_user", jsonutil::stringOr(player, "user_id")}}), [weak](auto r) {
            auto self = weak.lock();
            if (!self) return;
            setStatus(self->m_status, r.success ? "Jogador adicionado." : r.error, 390);
            if (r.success) self->fetch();
        });
    });
    if (picker) picker->show();
}

// ---------------------------------------------------------------- picker
PlayerPickerPopup* PlayerPickerPopup::create(std::string title, Pick pick) {
    auto p = new PlayerPickerPopup;
    if (p->init(std::move(title), std::move(pick))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool PlayerPickerPopup::init(std::string title, Pick pick) {
    if (!Popup::init(420, 270)) return false;
    visual::fitPopup(m_mainLayer, {420, 270});
    visual::stylePopup(m_mainLayer, m_bgSprite, {420, 270}, {36, 52, 96});
    setTitle(title, "goldFont.fnt", .6f, 16);
    m_pick = std::move(pick);
    m_input = TextInput::create(260, "Nome do jogador", "chatFont.fnt");
    m_input->setMaxCharCount(40);
    m_input->setCommonFilter(CommonFilter::Name);
    m_input->setPosition({165, 220});
    m_mainLayer->addChild(m_input, 4);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 4);
    auto search = ui::makeTextButton("Buscar", 90, "GJ_button_01.png", this, menu_selector(PlayerPickerPopup::onSearch), .34f, 30);
    search->setPosition({350, 220});
    menu->addChild(search);
    m_status = ui::makeLabel("", "chatFont.fnt", .38f, {180, 222, 248});
    m_status->setPosition({210, 14});
    m_mainLayer->addChild(m_status, 4);
    fetch();
    return true;
}

void PlayerPickerPopup::onSearch(CCObject*) { m_page = 0; m_input->defocus(); fetch(); }
void PlayerPickerPopup::onPage(CCObject* s) { if (!m_busy) { m_page = std::max(0, m_page + s->getTag()); fetch(); } }

void PlayerPickerPopup::fetch() {
    m_busy = true;
    setStatus(m_status, "Buscando...", 390);
    m_request.rpc("geobr_players", matjson::makeObject({{"p_query", std::string(m_input->getString().c_str())}, {"p_offset", m_page * 3}}), [this](auto r) {
        m_busy = false;
        if (r.success) { m_data = r.value; setStatus(m_status, "Toque no jogador", 390); }
        else setStatus(m_status, r.error, 390);
        render();
    });
}

void PlayerPickerPopup::render() {
    if (m_view) m_view->removeFromParent();
    m_view = CCNode::create();
    m_mainLayer->addChild(m_view, 3);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_view->addChild(menu);
    auto rows = m_data.get("players");
    int count = 0;
    if (rows && rows.unwrap().isArray()) for (auto const& p : rows.unwrap().asArray().unwrap()) {
        if (count >= 3) break;
        if (jsonutil::stringOr(p, "user_id") == CloudService::userID()) continue;
        auto row = playerRow(p, 340, fmt::format("Nv. {}  |  {}", levelOf(p), jsonutil::stringOr(p, "title_name", "Iniciante")), this, menu_selector(PlayerPickerPopup::onPick));
        row->setPosition({210, 165.f - count * 50});
        menu->addChild(row);
        ++count;
    }
    auto total = jsonutil::integerOr(m_data, "total");
    int pages = std::max<int64_t>(1, (total + 2) / 3);
    for (int side = 0; side < 2; ++side) {
        auto b = ui::makeTextButton(side ? ">" : "<", 40, "GJ_button_04.png", this, menu_selector(PlayerPickerPopup::onPage), .35f, 24);
        b->setTag(side ? 1 : -1);
        b->setPosition({side ? 380.f : 40.f, 40});
        b->setEnabled(!m_busy && (side ? m_page + 1 < pages : m_page > 0));
        b->setVisible(pages > 1);
        menu->addChild(b);
    }
}

void PlayerPickerPopup::onPick(CCObject* s) {
    auto id = static_cast<CCNode*>(s)->getID();
    auto rows = m_data.get("players");
    if (!rows || !rows.unwrap().isArray()) return;
    for (auto const& p : rows.unwrap().asArray().unwrap()) {
        if (jsonutil::stringOr(p, "user_id") == id) {
            if (m_pick) m_pick(p);
            onClose(nullptr);
            return;
        }
    }
}

// ------------------------------------------------------------ player card
PlayerCardPopup* PlayerCardPopup::create(matjson::Value player) {
    auto p = new PlayerCardPopup;
    if (p->init(std::move(player))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool PlayerCardPopup::init(matjson::Value player) {
    if (!Popup::init(440, 270)) return false;
    visual::fitPopup(m_mainLayer, {440, 270});
    m_player = std::move(player);
    auto style = jsonutil::stringOr(m_player, "theme_slug", "default");
    visual::stylePopup(m_mainLayer, m_bgSprite, {440, 270}, visual::palette(style).bottom);
    auto bg = visual::theme({426, 256}, style);
    bg->setPosition({7, 7});
    m_mainLayer->addChild(bg, -1);
    setID("geobr-player-card");
    label(m_mainLayer, jsonutil::stringOr(m_player, "display_name", "Jogador GeoBR"), {220, 244}, .6f, 380, {255, 230, 120});
    label(m_mainLayer, fmt::format("{}  |  Nv. {}", jsonutil::stringOr(m_player, "title_name", "Iniciante"), levelOf(m_player)), {220, 222}, .32f, 380, visual::palette(style).accent);
    // The whole icon kit as configured in GD: cube big, the others in a row.
    auto cube = visual::playerIcon(m_player, IconType::Cube, 56);
    cube->setPosition({220, 170});
    m_mainLayer->addChild(cube, 2);
    IconType const modes[] = {IconType::Ship, IconType::Ball, IconType::Ufo, IconType::Wave, IconType::Robot, IconType::Spider, IconType::Swing, IconType::Jetpack};
    for (int i = 0; i < 8; ++i) {
        auto icon = visual::playerIcon(m_player, modes[i], 30);
        icon->setPosition({220.f + (i - 3.5f) * 46.f, 112});
        m_mainLayer->addChild(icon, 2);
    }
    auto skin = static_cast<int>(jsonutil::integerOr(m_player, "arrow_skin", 1));
    arrow::Appearance look;
    look.skin = std::clamp(skin, 1, 16);
    auto decode = [](int64_t v) { return ccColor3B{static_cast<GLubyte>((v >> 16) & 255), static_cast<GLubyte>((v >> 8) & 255), static_cast<GLubyte>(v & 255)}; };
    look.primary = decode(jsonutil::integerOr(m_player, "color1", 0x42e0ff));
    look.secondary = decode(jsonutil::integerOr(m_player, "color2", 0xa155ff));
    look.glow = decode(jsonutil::integerOr(m_player, "glow_color", 0xffffff));
    look.glowEnabled = jsonutil::boolOr(m_player, "glow", false);
    if (auto arrowIcon = arrow::ArrowIcon::create(look)) {
        arrowIcon->setRotation(-45.f);
        arrowIcon->setScale(1.2f);
        arrowIcon->setPosition({300, 170});
        m_mainLayer->addChild(arrowIcon, 2);
        label(m_mainLayer, "Arrow", {300, 140}, .22f, 80, {255, 200, 140});
    }
    bool self = jsonutil::stringOr(m_player, "user_id") == CloudService::userID();
    if (!self) {
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu, 4);
        auto dm = ui::makeTextButton("Mensagem", 130, "GJ_button_01.png", this, menu_selector(PlayerCardPopup::onMessage), .32f, 30);
        dm->setPosition({145, 45});
        menu->addChild(dm);
        auto group = ui::makeTextButton("Adicionar a grupo", 170, "GJ_button_04.png", this, menu_selector(PlayerCardPopup::onGroup), .3f, 30);
        group->setPosition({305, 45});
        menu->addChild(group);
    }
    else {
        label(m_mainLayer, "Este e voce. Mude o icone na garagem do GD.", {220, 45}, .45f, 380, {190, 214, 240}, false, "chatFont.fnt");
    }
    return true;
}

void PlayerCardPopup::onMessage(CCObject*) {
    if (auto p = DirectMessagesPopup::create(jsonutil::stringOr(m_player, "user_id"))) p->show();
}

void PlayerCardPopup::onGroup(CCObject*) {
    if (auto p = GroupsPopup::create(jsonutil::stringOr(m_player, "user_id"), jsonutil::stringOr(m_player, "display_name"))) p->show();
}

// ----------------------------------------------------------------- groups
GroupsPopup* GroupsPopup::create(std::string addUser, std::string addName) {
    auto p = new GroupsPopup;
    if (p->init(std::move(addUser), std::move(addName))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool GroupsPopup::init(std::string addUser, std::string addName) {
    if (!Popup::init(470, 300)) return false;
    visual::fitPopup(m_mainLayer, {470, 300});
    visual::stylePopup(m_mainLayer, m_bgSprite, {470, 300}, {53, 39, 78});
    auto bg = visual::theme({456, 286}, "cosmos");
    bg->setPosition({7, 7});
    m_mainLayer->addChild(bg, -1);
    m_addUser = std::move(addUser);
    m_addName = std::move(addName);
    setTitle(m_addUser.empty() ? "Grupos" : "Adicionar a qual grupo?", "goldFont.fnt", .65f, 16);
    setID("geobr-groups");
    m_status = ui::makeLabel("Carregando...", "chatFont.fnt", .38f, {180, 222, 248});
    m_status->setPosition({235, 13});
    m_mainLayer->addChild(m_status, 5);
    if (m_addUser.empty()) {
        m_input = TextInput::create(250, "Nome do novo grupo", "chatFont.fnt");
        m_input->setMaxCharCount(32);
        m_input->setCommonFilter(CommonFilter::Any);
        m_input->setPosition({170, 42});
        m_mainLayer->addChild(m_input, 4);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu, 4);
        auto create = ui::makeTextButton("Criar grupo", 120, "GJ_button_01.png", this, menu_selector(GroupsPopup::onCreate), .32f, 30);
        create->setPosition({375, 42});
        create->setID("geobr-group-create");
        menu->addChild(create);
    }
    fetch();
    return true;
}

void GroupsPopup::fetch() {
    m_busy = true;
    m_fetch.rpc("geobr_group_list", matjson::Value::object(), [this](auto r) {
        m_busy = false;
        if (!r.success) { setStatus(m_status, r.error); return; }
        m_groups = r.value;
        if (!m_selected.isNull()) {
            auto id = jsonutil::stringOr(m_selected, "id");
            m_selected = nullptr;
            for (auto const& g : arrayOrEmpty(m_groups).asArray().unwrap()) if (jsonutil::stringOr(g, "id") == id) m_selected = g;
        }
        render();
    });
}

void GroupsPopup::render() {
    if (m_view) m_view->removeFromParent();
    m_view = CCNode::create();
    m_mainLayer->addChild(m_view, 3);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_view->addChild(menu);
    if (m_input) m_input->setVisible(m_selected.isNull());
    if (auto create = m_mainLayer->getChildByIDRecursive("geobr-group-create")) create->setVisible(m_selected.isNull());

    if (!m_selected.isNull()) {
        // One group: chat, members, leave.
        auto name = jsonutil::stringOr(m_selected, "name", "Grupo");
        label(m_view, name, {235, 225}, .6f, 400, {255, 230, 120});
        label(m_view, fmt::format("{} membro{}  |  voce e {}", jsonutil::integerOr(m_selected, "members"), jsonutil::integerOr(m_selected, "members") == 1 ? "" : "s",
            jsonutil::stringOr(m_selected, "role") == "owner" ? "dono" : "membro"), {235, 200}, .3f, 400, {180, 220, 250});
        char const* labels[] = {"Chat do grupo", "Membros", "Sair do grupo", "Voltar"};
        char const* colors[] = {"GJ_button_01.png", "GJ_button_02.png", "GJ_button_06.png", "GJ_button_04.png"};
        SEL_MenuHandler handlers[] = {menu_selector(GroupsPopup::onChat), menu_selector(GroupsPopup::onMembers), menu_selector(GroupsPopup::onLeave), menu_selector(GroupsPopup::onBack)};
        for (int i = 0; i < 4; ++i) {
            auto b = ui::makeTextButton(labels[i], 170, colors[i], this, handlers[i], .32f, 32);
            b->setPosition({140.f + (i % 2) * 190.f, 150.f - (i / 2) * 46.f});
            menu->addChild(b);
        }
        setStatus(m_status, "O chat do grupo e so para os membros.");
        return;
    }

    auto rows = arrayOrEmpty(m_groups).asArray().unwrap();
    int pages = std::max<int>(1, (static_cast<int>(rows.size()) + 3) / 4);
    m_page = std::clamp(m_page, 0, pages - 1);
    for (int i = 0; i < 4; ++i) {
        size_t index = m_page * 4 + i;
        if (index >= rows.size()) break;
        auto const& g = rows[index];
        auto row = CCNode::create();
        row->setContentSize({400, 40});
        auto panel = ui::makePanel({400, 40}, {22, 32, 62}, 245);
        panel->setPosition({200, 20});
        row->addChild(panel);
        label(row, jsonutil::stringOr(g, "name", "Grupo"), {14, 27}, .38f, 250, {255, 255, 255}, true);
        auto preview = jsonutil::stringOr(g, "last_body");
        label(row, preview.empty() ? std::string("Sem mensagens ainda") : preview, {14, 11}, .4f, 300, {170, 205, 240}, true, "chatFont.fnt");
        label(row, fmt::format("{} pessoa{}", jsonutil::integerOr(g, "members"), jsonutil::integerOr(g, "members") == 1 ? "" : "s"), {360, 20}, .26f, 70, {255, 222, 134});
        auto button = CCMenuItemSpriteExtra::create(row, this, menu_selector(GroupsPopup::onGroup));
        button->setTag(static_cast<int>(index));
        button->setPosition({235, 225.f - i * 46});
        menu->addChild(button);
    }
    if (rows.empty()) label(m_view, m_busy ? "Carregando..." : "Voce ainda nao tem grupos.", {235, 160}, .4f, 400, {204, 225, 247});
    if (pages > 1) {
        for (int side = 0; side < 2; ++side) {
            auto b = ui::makeTextButton(side ? ">" : "<", 40, "GJ_button_04.png", this, menu_selector(GroupsPopup::onPage), .35f, 24);
            b->setTag(side ? 1 : -1);
            b->setPosition({side ? 445.f : 25.f, 160});
            b->setEnabled(side ? m_page + 1 < pages : m_page > 0);
            menu->addChild(b);
        }
    }
    setStatus(m_status, m_addUser.empty() ? "Toque num grupo para abrir o chat e os membros." : "Toque no grupo para adicionar " + m_addName + ".");
}

void GroupsPopup::onPage(CCObject* s) { m_page += s->getTag(); render(); }

void GroupsPopup::onGroup(CCObject* s) {
    auto rows = arrayOrEmpty(m_groups).asArray().unwrap();
    size_t index = static_cast<size_t>(s->getTag());
    if (index >= rows.size()) return;
    if (!m_addUser.empty()) {
        if (m_busy) return;
        m_busy = true;
        m_action.rpc("geobr_group_add", matjson::makeObject({{"p_group", jsonutil::stringOr(rows[index], "id")}, {"p_user", m_addUser}}), [this](auto r) {
            m_busy = false;
            setStatus(m_status, r.success ? m_addName + " foi adicionado ao grupo." : r.error);
            if (r.success) fetch();
        });
        return;
    }
    m_selected = rows[index];
    render();
}

void GroupsPopup::onCreate(CCObject*) {
    if (m_busy || !m_input) return;
    auto name = text::sanitizeMessage(m_input->getString().c_str(), 32);
    if (name.size() < 3) { setStatus(m_status, "O nome precisa de 3 a 32 caracteres."); return; }
    m_busy = true;
    setStatus(m_status, "Criando grupo...");
    m_action.rpc("geobr_group_create", matjson::makeObject({{"p_name", name}}), [this](auto r) {
        m_busy = false;
        if (!r.success) { setStatus(m_status, r.error); return; }
        m_input->setString("");
        m_selected = r.value;
        setStatus(m_status, "Grupo criado. Adicione jogadores em Membros.");
        fetch();
    });
}

void GroupsPopup::onBack(CCObject*) { m_selected = nullptr; render(); }

void GroupsPopup::onChat(CCObject*) {
    if (m_selected.isNull()) return;
    if (auto p = ChatPopup::create(ChatKind::Group, jsonutil::stringOr(m_selected, "id"), jsonutil::stringOr(m_selected, "name", "Grupo"))) p->show();
}

void GroupsPopup::onMembers(CCObject*) {
    if (m_selected.isNull()) return;
    if (auto p = MembersPopup::create(ChatKind::Group, jsonutil::stringOr(m_selected, "id"), jsonutil::stringOr(m_selected, "name", "Membros"))) p->show();
}

void GroupsPopup::onLeave(CCObject*) {
    if (m_selected.isNull()) return;
    auto id = jsonutil::stringOr(m_selected, "id");
    WeakRef<GroupsPopup> weak = this;
    createQuickPopup("Sair do grupo?", "Voce deixa de ver o chat. Se for o dono, o grupo passa para o membro mais antigo.", "Cancelar", "Sair",
        [weak, id](auto, bool yes) {
            auto self = weak.lock();
            if (!yes || !self) return;
            self->m_action.rpc("geobr_group_leave", matjson::makeObject({{"p_group", id}}), [weak](auto r) {
                auto self = weak.lock();
                if (!self) return;
                setStatus(self->m_status, r.success ? "Voce saiu do grupo." : r.error);
                if (r.success) { self->m_selected = nullptr; self->fetch(); }
            });
        });
}

} // namespace geobr
