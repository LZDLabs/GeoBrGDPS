#include "CollabPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "SocialPopups.hpp"
#include "UIHelpers.hpp"
#include "../collab/CollabService.hpp"
#include "../models/JsonModelUtils.hpp"
#include "../utils/TextSanitizer.hpp"

#include <Geode/binding/EditLevelLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LocalLevelManager.hpp>
#include <Geode/ui/Notification.hpp>

#include <algorithm>
#include <cctype>

using namespace geode::prelude;

namespace geobr {
namespace {

constexpr CCSize Size {480, 310};

void label(CCNode* parent, std::string const& text, CCPoint pos, float scale, float width, ccColor3B color = {255, 255, 255}, bool left = false, char const* font = "bigFont.fnt") {
    auto l = ui::makeLabel(text, font, scale, color);
    l->setAnchorPoint({left ? 0.f : .5f, .5f});
    l->setPosition(pos);
    l->limitLabelWidth(width, scale, .18f);
    parent->addChild(l);
}

void setStatus(CCLabelBMFont* status, std::string const& text) {
    if (!status) return;
    status->setString(text.c_str());
    status->limitLabelWidth(450, .38f, .22f);
}

std::vector<GJGameLevel*> localLevels() {
    std::vector<GJGameLevel*> out;
    if (auto levels = LocalLevelManager::sharedState()->m_localLevels) {
        for (auto level : CCArrayExt<GJGameLevel*>(levels)) if (level) out.push_back(level);
    }
    return out;
}

matjson::Value arrayOrEmpty(matjson::Value const& value) { return value.isArray() ? value : matjson::Value::array(); }

} // namespace

CollabPopup* CollabPopup::create() {
    auto p = new CollabPopup;
    if (p->init()) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool CollabPopup::init() {
    if (!Popup::init(Size.width, Size.height)) return false;
    visual::fitPopup(m_mainLayer, Size);
    visual::stylePopup(m_mainLayer, m_bgSprite, Size, {53, 39, 78});
    auto bg = visual::theme({Size.width - 14, Size.height - 14}, "cosmos");
    bg->setPosition({7, 7});
    m_mainLayer->addChild(bg, -1);
    setTitle("Editor Collab", "goldFont.fnt", .7f, 16);
    setID("geobr-collab");
    m_status = ui::makeLabel("Carregando salas...", "chatFont.fnt", .38f, {180, 222, 248});
    m_status->setPosition({Size.width / 2, 13});
    m_mainLayer->addChild(m_status, 5);
    m_code = TextInput::create(170, "Codigo (8)", "bigFont.fnt");
    m_code->setMaxCharCount(8);
    m_code->setCommonFilter(CommonFilter::Alphanumeric);
    m_code->setPosition({120, 44});
    m_mainLayer->addChild(m_code, 4);
    m_title = TextInput::create(260, "Nome da sala", "chatFont.fnt");
    m_title->setMaxCharCount(40);
    m_title->setCommonFilter(CommonFilter::Any);
    m_title->setPosition({Size.width / 2, 236});
    m_mainLayer->addChild(m_title, 4);
    fetch();
    return true;
}

// A tap while something is pending says so (it used to return silently), and a
// request with no answer for 20 s no longer blocks the button for good.
bool CollabPopup::startAction(bool& flag, std::time_t& since, char const* message) {
    auto now = std::time(nullptr);
    if (flag && now - since < 20) { setStatus(m_status, "Aguarde, ainda processando..."); return false; }
    flag = true;
    since = now;
    setStatus(m_status, message);
    return true;
}

void CollabPopup::fetch() {
    m_fetch.rpc("geobr_collab_rooms", matjson::Value::object(), [this](auto r) {
        if (!r.success) { setStatus(m_status, r.error); render(); return; }
        m_rooms = r.value;
        auto rooms = arrayOrEmpty(m_rooms).asArray().unwrap();
        collab::CollabService::get()->setRoomCount(static_cast<int>(rooms.size()));
        if (!m_selected.isNull()) {
            auto id = jsonutil::stringOr(m_selected, "id");
            m_selected = nullptr;
            for (auto const& room : rooms) if (jsonutil::stringOr(room, "id") == id) m_selected = room;
            if (m_selected.isNull() && m_view == View::Detail) m_view = View::List;
        }
        render();
    });
}

void CollabPopup::render() {
    if (m_content) m_content->removeFromParent();
    m_content = CCNode::create();
    m_mainLayer->addChild(m_content, 3);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_content->addChild(menu);
    m_code->setVisible(m_view == View::List);
    m_title->setVisible(m_view == View::Create);
    switch (m_view) {
        case View::List: renderList(menu); break;
        case View::Detail: renderDetail(menu); break;
        case View::Create: renderCreate(menu); break;
    }
}

void CollabPopup::renderList(CCMenu* menu) {
    auto rooms = arrayOrEmpty(m_rooms).asArray().unwrap();
    int pages = std::max<int>(1, (static_cast<int>(rooms.size()) + 3) / 4);
    m_page = std::clamp(m_page, 0, pages - 1);
    for (int i = 0; i < 4; ++i) {
        size_t index = m_page * 4 + i;
        if (index >= rooms.size()) break;
        auto const& room = rooms[index];
        auto row = CCNode::create();
        row->setContentSize({420, 42});
        auto panel = ui::makePanel({420, 42}, {22, 32, 62}, 245);
        panel->setPosition({210, 21});
        row->addChild(panel);
        if (auto icon = CCSprite::create("geobr-collab-icon.png"_spr)) {
            icon->setScale(32.f / icon->getContentWidth());
            icon->setPosition({22, 21});
            row->addChild(icon);
        }
        label(row, jsonutil::stringOr(room, "title", "Collab"), {44, 28}, .38f, 230, {255, 255, 255}, true);
        auto posted = jsonutil::integerOr(room, "gd_level_id");
        label(row, fmt::format("{} pessoa(s)  |  revisao {}{}", jsonutil::integerOr(room, "members"), jsonutil::integerOr(room, "revision"),
            posted > 0 ? fmt::format("  |  publicado: {}", posted) : std::string()), {44, 11}, .42f, 260, {170, 205, 240}, true, "chatFont.fnt");
        label(row, jsonutil::stringOr(room, "code"), {372, 21}, .34f, 80, {255, 222, 134});
        auto button = CCMenuItemSpriteExtra::create(row, this, menu_selector(CollabPopup::onRoom));
        button->setTag(static_cast<int>(index));
        button->setPosition({Size.width / 2, 245.f - i * 47});
        menu->addChild(button);
    }
    if (rooms.empty()) {
        label(m_content, m_rooms.isNull() ? "Carregando..." : "Voce ainda nao esta em nenhum collab.", {Size.width / 2, 190}, .4f, 420, {204, 225, 247});
        label(m_content, "Crie uma sala e mande o codigo, ou entre com o codigo de alguem.", {Size.width / 2, 165}, .45f, 420, {170, 205, 240}, false, "chatFont.fnt");
    }
    if (pages > 1) {
        for (int side = 0; side < 2; ++side) {
            auto b = ui::makeTextButton(side ? ">" : "<", 36, "GJ_button_04.png", this, menu_selector(CollabPopup::onPage), .35f, 24);
            b->setTag(side ? 1 : -1);
            b->setPosition({side ? 458.f : 22.f, 175});
            b->setEnabled(side ? m_page + 1 < pages : m_page > 0);
            menu->addChild(b);
        }
    }
    auto join = ui::makeTextButton("Entrar", 80, "GJ_button_01.png", this, menu_selector(CollabPopup::onJoin), .32f, 30);
    join->setPosition({255, 44});
    menu->addChild(join);
    auto create = ui::makeTextButton("Criar sala", 120, "GJ_button_02.png", this, menu_selector(CollabPopup::onNew), .32f, 30);
    create->setPosition({390, 44});
    menu->addChild(create);
    if (!m_rooms.isNull()) setStatus(m_status, "As salas tambem aparecem em Meus Levels, no botao My Collabs.");
}

void CollabPopup::renderDetail(CCMenu* menu) {
    auto const& room = m_selected;
    label(m_content, jsonutil::stringOr(room, "title", "Collab"), {Size.width / 2, 252}, .58f, 420, {255, 230, 120});
    label(m_content, "Codigo para convidar:", {Size.width / 2, 226}, .28f, 300, {180, 220, 250});
    label(m_content, jsonutil::stringOr(room, "code"), {Size.width / 2 - 40, 200}, .8f, 220, {255, 255, 255});
    auto copy = ui::makeTextButton("Copiar", 80, "GJ_button_04.png", this, menu_selector(CollabPopup::onCopy), .3f, 26);
    copy->setPosition({Size.width / 2 + 110, 200});
    menu->addChild(copy);
    auto role = jsonutil::stringOr(room, "role");
    auto posted = jsonutil::integerOr(room, "gd_level_id");
    label(m_content, fmt::format("{} pessoa(s)  |  revisao {}  |  voce e {}{}", jsonutil::integerOr(room, "members"), jsonutil::integerOr(room, "revision"),
        role == "owner" ? "dono" : role == "viewer" ? "so leitura" : "editor", posted > 0 ? fmt::format("  |  ID no GD: {}", posted) : std::string()),
        {Size.width / 2, 172}, .45f, 440, {170, 205, 240}, false, "chatFont.fnt");
    char const* labels[] = {"Abrir level", "Chat", "Membros", "Sair da sala", "Voltar"};
    char const* colors[] = {"GJ_button_01.png", "GJ_button_02.png", "GJ_button_02.png", "GJ_button_06.png", "GJ_button_04.png"};
    SEL_MenuHandler handlers[] = {menu_selector(CollabPopup::onOpen), menu_selector(CollabPopup::onChat), menu_selector(CollabPopup::onMembers),
        menu_selector(CollabPopup::onLeave), menu_selector(CollabPopup::onBack)};
    CCPoint positions[] = {{Size.width / 2, 128}, {140, 84}, {340, 84}, {140, 44}, {340, 44}};
    for (int i = 0; i < 5; ++i) {
        auto b = ui::makeTextButton(labels[i], i == 0 ? 200 : 150, colors[i], this, handlers[i], .32f, 30);
        b->setPosition(positions[i]);
        menu->addChild(b);
    }
    setStatus(m_status, "No editor: Sync envia/recebe as mudancas; salvar tambem envia.");
}

void CollabPopup::renderCreate(CCMenu* menu) {
    label(m_content, "Comecar a partir de:", {Size.width / 2, 205}, .3f, 300, {180, 220, 250});
    auto levels = localLevels();
    int perPage = 4;
    int total = static_cast<int>(levels.size()) + 1;  // index 0 = empty level
    int pages = std::max(1, (total + perPage - 1) / perPage);
    m_levelPage = std::clamp(m_levelPage, 0, pages - 1);
    for (int i = 0; i < perPage; ++i) {
        int index = m_levelPage * perPage + i;
        if (index >= total) break;
        std::string name = index == 0 ? std::string("Level vazio (novo)") : std::string(levels[index - 1]->m_levelName);
        bool chosen = m_source == index - 1;
        auto b = ui::makeTextButton(name.empty() ? std::string("Sem nome") : name, 200, chosen ? "GJ_button_01.png" : "GJ_button_04.png", this, menu_selector(CollabPopup::onSource), .28f, 26);
        b->setTag(index - 1);
        b->setPosition({135.f + (i % 2) * 210.f, 172.f - (i / 2) * 34.f});
        menu->addChild(b);
    }
    if (pages > 1) {
        for (int side = 0; side < 2; ++side) {
            auto b = ui::makeTextButton(side ? ">" : "<", 36, "GJ_button_04.png", this, menu_selector(CollabPopup::onLevelPage), .35f, 24);
            b->setTag(side ? 1 : -1);
            b->setPosition({side ? 458.f : 22.f, 155});
            b->setEnabled(side ? m_levelPage + 1 < pages : m_levelPage > 0);
            menu->addChild(b);
        }
    }
    label(m_content, "Seu level original nao muda: a sala usa uma copia.", {Size.width / 2, 100}, .42f, 440, {170, 205, 240}, false, "chatFont.fnt");
    auto create = ui::makeTextButton("Criar sala", 150, "GJ_button_01.png", this, menu_selector(CollabPopup::onCreate), .32f, 30);
    create->setPosition({320, 52});
    menu->addChild(create);
    auto back = ui::makeTextButton("Voltar", 100, "GJ_button_04.png", this, menu_selector(CollabPopup::onBack), .3f, 30);
    back->setPosition({150, 52});
    menu->addChild(back);
    setStatus(m_status, "Ate 8 pessoas por sala; o codigo convida qualquer jogador GeoBR.");
}

void CollabPopup::onPage(CCObject* s) { m_page += s->getTag(); render(); }
void CollabPopup::onLevelPage(CCObject* s) { m_levelPage += s->getTag(); render(); }

void CollabPopup::onRoom(CCObject* s) {
    auto rooms = arrayOrEmpty(m_rooms).asArray().unwrap();
    size_t index = static_cast<size_t>(s->getTag());
    if (index >= rooms.size()) return;
    m_selected = rooms[index];
    m_view = View::Detail;
    render();
}

void CollabPopup::onJoin(CCObject*) {
    // Accepts "abcd efgh", "ABCD-EFGH" or a pasted code with spaces.
    std::string code;
    for (unsigned char c : std::string(m_code->getString())) if (std::isalnum(c)) code += static_cast<char>(std::toupper(c));
    if (code.size() != 8) { setStatus(m_status, fmt::format("O codigo tem 8 letras/numeros (voce digitou {}).", code.size())); return; }
    if (!startAction(m_joining, m_joinSince, "Entrando na sala...")) return;
    log::info("Collab: joining room with code {}", code);
    m_join.rpc("geobr_collab_join", matjson::makeObject({{"p_code", code}}), [this](auto r) {
        m_joining = false;
        if (!r.success) log::warn("Collab: join failed: {}", r.error);
        if (!r.success) { setStatus(m_status, r.error); return; }
        if (!jsonutil::boolOr(r.value, "ok")) { setStatus(m_status, jsonutil::stringOr(r.value, "error", "Nao foi possivel entrar.")); return; }
        m_code->setString("");
        if (auto room = r.value.get("room")) m_selected = room.unwrap();
        m_view = View::Detail;
        setStatus(m_status, "Voce entrou na sala!");
        fetch();
    });
}

void CollabPopup::onNew(CCObject*) { m_view = View::Create; m_source = -1; render(); }
void CollabPopup::onSource(CCObject* s) { m_source = s->getTag(); render(); }

void CollabPopup::onCreate(CCObject*) {
    auto title = text::sanitizeMessage(m_title->getString().c_str(), 40);
    auto levels = localLevels();
    if (title.empty() && m_source >= 0 && m_source < static_cast<int>(levels.size())) title = std::string(levels[m_source]->m_levelName);
    if (title.empty()) { setStatus(m_status, "De um nome para a sala."); return; }
    std::string level;
    if (m_source >= 0 && m_source < static_cast<int>(levels.size())) {
        // A copy of the chosen level, without any previous room tag.
        auto plain = collab::CollabService::decompress(std::string(levels[m_source]->m_levelString));
        if (!plain.empty()) level = collab::CollabService::compress(plain);
    }
    if (!startAction(m_busy, m_busySince, "Criando sala...")) return;
    m_action.rpc("geobr_collab_create", matjson::makeObject({{"p_title", title}, {"p_level", level}}), [this](auto r) {
        m_busy = false;
        if (!r.success) { setStatus(m_status, r.error); return; }
        m_selected = r.value;
        m_title->setString("");
        m_view = View::Detail;
        collab::CollabService::get()->setRoomCount(std::max(1, static_cast<int>(arrayOrEmpty(m_rooms).asArray().unwrap().size()) + 1));
        setStatus(m_status, "Sala criada! Envie o codigo para quem vai editar com voce.");
        fetch();
    });
}

void CollabPopup::onBack(CCObject*) { m_view = View::List; m_selected = nullptr; render(); }

void CollabPopup::onCopy(CCObject*) {
    auto code = jsonutil::stringOr(m_selected, "code");
    if (code.empty()) return;
    utils::clipboard::write(code);
    Notification::create("Codigo copiado", NotificationIcon::Success, 1.5f)->show();
}

void CollabPopup::onOpen(CCObject*) {
    if (m_selected.isNull()) return;
    if (!startAction(m_busy, m_busySince, "Baixando a versao mais recente...")) return;
    WeakRef<CollabPopup> weak = this;
    collab::CollabService::get()->openRoom(m_selected, [weak](GJGameLevel* level, std::string error) {
        auto self = weak.lock();
        if (self) self->m_busy = false;
        if (!level) {
            if (self) setStatus(self->m_status, error);
            else Notification::create(error, NotificationIcon::Error, 2.f)->show();
            return;
        }
        // GD's own page for the level: Edit, Play and the usual options.
        CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(.4f, EditLevelLayer::scene(level)));
    });
}

void CollabPopup::onChat(CCObject*) {
    if (m_selected.isNull()) return;
    if (auto p = ChatPopup::create(ChatKind::Collab, jsonutil::stringOr(m_selected, "id"), jsonutil::stringOr(m_selected, "title", "Chat do collab"))) p->show();
}

void CollabPopup::onMembers(CCObject*) {
    if (m_selected.isNull()) return;
    if (auto p = MembersPopup::create(ChatKind::Collab, jsonutil::stringOr(m_selected, "id"), "Membros do collab")) p->show();
}

void CollabPopup::onLeave(CCObject*) {
    if (m_selected.isNull()) return;
    auto id = jsonutil::stringOr(m_selected, "id");
    WeakRef<CollabPopup> weak = this;
    createQuickPopup("Sair da sala?", "Sua copia local fica no aparelho. Se voce for o dono, a sala passa para o membro mais antigo.", "Cancelar", "Sair",
        [weak, id](auto, bool yes) {
            auto self = weak.lock();
            if (!yes || !self) return;
            self->m_action.rpc("geobr_collab_leave", matjson::makeObject({{"p_room", id}}), [weak](auto r) {
                auto self = weak.lock();
                if (!self) return;
                setStatus(self->m_status, r.success ? "Voce saiu da sala." : r.error);
                if (r.success) { self->m_selected = nullptr; self->m_view = View::List; self->fetch(); }
            });
        });
}

// ---------------------------------------------------------------- credits
CollabCreditsPopup* CollabCreditsPopup::create(int levelID, std::string levelName) {
    auto p = new CollabCreditsPopup;
    if (p->init(levelID, std::move(levelName))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool CollabCreditsPopup::init(int levelID, std::string levelName) {
    if (!Popup::init(400, 280)) return false;
    visual::fitPopup(m_mainLayer, {400, 280});
    visual::stylePopup(m_mainLayer, m_bgSprite, {400, 280}, {36, 52, 96});
    setTitle("Collab", "goldFont.fnt", .65f, 16);
    setID("geobr-collab-credits");
    label(m_mainLayer, levelName, {200, 232}, .42f, 360, {255, 230, 120});
    m_status = ui::makeLabel("Carregando...", "chatFont.fnt", .4f, {180, 222, 248});
    m_status->setPosition({200, 16});
    m_mainLayer->addChild(m_status, 4);
    m_fetch.rpc("geobr_collab_credits", matjson::makeObject({{"p_level_id", levelID}}), [this](auto r) {
        if (!r.success) { setStatus(m_status, r.error); return; }
        if (!jsonutil::boolOr(r.value, "found")) { setStatus(m_status, "Este level nao e um collab GeoBR."); return; }
        if (auto members = r.value.get("members")) m_members = members.unwrap();
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu, 3);
        auto rows = arrayOrEmpty(m_members).asArray().unwrap();
        int shown = 0;
        for (size_t i = 0; i < rows.size() && shown < 8; ++i, ++shown) {
            auto const& m = rows[i];
            auto cell = CCNode::create();
            cell->setContentSize({170, 40});
            auto panel = ui::makePanel({170, 40}, {22, 32, 62}, 245);
            panel->setPosition({85, 20});
            cell->addChild(panel);
            auto icon = visual::avatar(m, 26);
            icon->setPosition({20, 20});
            cell->addChild(icon, 2);
            label(cell, jsonutil::stringOr(m, "display_name", "Jogador"), {38, 26}, .3f, 120, {255, 255, 255}, true);
            label(cell, jsonutil::stringOr(m, "role") == "owner" ? "Dono da sala" : "Ajudou", {38, 11}, .22f, 120, {170, 205, 240}, true);
            auto button = CCMenuItemSpriteExtra::create(cell, this, menu_selector(CollabCreditsPopup::onPlayer));
            button->setTag(static_cast<int>(i));
            button->setPosition({110.f + (shown % 2) * 180.f, 198.f - (shown / 2) * 44.f});
            menu->addChild(button);
        }
        setStatus(m_status, fmt::format("Sala \"{}\": {} pessoa(s) fizeram este level", jsonutil::stringOr(r.value, "title"), rows.size()));
    });
    return true;
}

void CollabCreditsPopup::onPlayer(CCObject* s) {
    auto rows = arrayOrEmpty(m_members).asArray().unwrap();
    size_t index = static_cast<size_t>(s->getTag());
    if (index < rows.size()) if (auto p = PlayerCardPopup::create(rows[index])) p->show();
}

} // namespace geobr
