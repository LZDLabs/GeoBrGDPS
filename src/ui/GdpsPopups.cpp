#include "GdpsPopups.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../models/JsonModelUtils.hpp"

#include <Geode/binding/AccountLayer.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/LevelBrowserLayer.hpp>
#include <Geode/binding/MusicDownloadManager.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/web.hpp>

#include <filesystem>

using namespace geode::prelude;
using namespace geode::utils::web;

namespace geobr {
namespace {

void frame(geode::Popup* popup, CCNode* main, CCNode* bg, CCSize size, char const* style = "cosmos") {
    visual::fitPopup(main, size);
    visual::stylePopup(main, bg, size, {44, 38, 86});
    auto backdrop = visual::theme({size.width - 14, size.height - 14}, style);
    backdrop->setPosition({7, 7});
    main->addChild(backdrop, -1);
    (void)popup;
}

CCLabelBMFont* text(CCNode* parent, std::string const& value, CCPoint pos, float scale, float width,
                    ccColor3B color = {255, 255, 255}, char const* font = "bigFont.fnt", bool left = false) {
    auto label = ui::makeLabel(value, font, scale, color);
    label->setAnchorPoint({left ? 0.f : .5f, .5f});
    label->setPosition(pos);
    label->limitLabelWidth(width, scale, .15f);
    parent->addChild(label, 3);
    return label;
}

void setText(CCLabelBMFont* label, std::string const& value, float width, float scale) {
    if (!label) return;
    label->setString(value.c_str());
    label->limitLabelWidth(width, scale, .15f);
}

ccColor3B rgb(int64_t color) {
    return {static_cast<GLubyte>((color >> 16) & 255), static_cast<GLubyte>((color >> 8) & 255), static_cast<GLubyte>(color & 255)};
}

std::vector<matjson::Value> items(matjson::Value const& value, char const* key) {
    std::vector<matjson::Value> out;
    auto list = value.get(key);
    if (list && list.unwrap().isArray()) for (auto const& v : list.unwrap().asArray().unwrap()) out.push_back(v);
    return out;
}

bool danger(std::string const& label) {
    return label == "Ban" || label == "Apagar" || label == "Remover" || label == "Bloquear" || label == "LB ban" || label == "Zerar";
}

} // namespace

// --------------------------------------------------------------- prompt
GdpsPrompt* GdpsPrompt::create(std::string const& title, std::string const& body, std::vector<Field> fields,
                               std::vector<std::string> buttons, Done done) {
    auto p = new GdpsPrompt;
    if (p->init(title, body, std::move(fields), std::move(buttons), std::move(done))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool GdpsPrompt::init(std::string const& title, std::string const& body, std::vector<Field> fields, std::vector<std::string> buttons, Done done) {
    // Many fields (e.g. one per level ID) go in two columns so the form fits phone and PC screens.
    bool twoColumns = fields.size() > 5;
    size_t rows = twoColumns ? (fields.size() + 1) / 2 : fields.size();
    float spacing = rows > 4 ? 30.f : 38.f;
    float height = 96.f + spacing * rows + (body.empty() ? 0.f : 26.f);
    CCSize size {twoColumns ? 470.f : 380.f, height};
    if (!Popup::init(size.width, size.height)) return false;
    frame(this, m_mainLayer, m_bgSprite, size);
    setTitle(title, "goldFont.fnt", .6f, 15);
    m_done = std::move(done);
    float y = height - 44.f;
    if (!body.empty()) {
        text(m_mainLayer, body, {size.width / 2, y}, .5f, size.width - 30.f, ui::palette::TextSoft, "chatFont.fnt");
        y -= 26.f;
    }
    for (size_t i = 0; i < fields.size(); ++i) {
        auto const& f = fields[i];
        float rowY = twoColumns ? y - float(i % rows) * spacing : y - float(i) * spacing;
        float left = twoColumns && i >= rows ? 240.f : 0.f;
        float labelWidth = twoColumns ? 78.f : 120.f, inputWidth = twoColumns ? 130.f : 210.f;
        text(m_mainLayer, f.label, {left + 16, rowY}, .42f, labelWidth, {216, 236, 255}, "chatFont.fnt", true);
        auto input = TextInput::create(inputWidth, f.label);
        input->setCommonFilter(f.filter);
        input->setMaxCharCount(f.max);
        input->setString(f.value);
        if (f.password) input->setPasswordMode(true);
        input->setPosition({twoColumns ? left + 160.f : 252.f, rowY});
        m_mainLayer->addChild(input, 4);
        m_inputs.push_back(input);
    }
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    float width = std::min(110.f, (size.width - 30.f) / std::max<size_t>(1, buttons.size()) - 8.f);
    for (size_t i = 0; i < buttons.size(); ++i) {
        auto b = ui::makeTextButton(buttons[i], int(width), danger(buttons[i]) ? "GJ_button_06.png" : (i == 0 ? "GJ_button_01.png" : "GJ_button_04.png"),
                                    this, menu_selector(GdpsPrompt::onButton), .34f, 26);
        b->setTag(int(i));
        b->setPosition({size.width / 2 + (float(i) - (buttons.size() - 1) / 2.f) * (width + 8.f), 24});
        menu->addChild(b);
    }
    m_mainLayer->addChild(menu, 5);
    return true;
}

void GdpsPrompt::onButton(CCObject* sender) {
    std::vector<std::string> values;
    for (auto input : m_inputs) values.push_back(std::string(input->getString()));
    auto done = m_done;
    int button = sender->getTag();
    onClose(nullptr);
    if (done) done(button, values);
}

// ----------------------------------------------------------------- list
GdpsList* GdpsList::create(CCSize size, Pressed pressed) {
    auto p = new GdpsList;
    if (p->init(size, std::move(pressed))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool GdpsList::init(CCSize size, Pressed pressed) {
    if (!CCNode::init()) return false;
    m_size = size;
    m_pressed = std::move(pressed);
    setContentSize(size);
    setAnchorPoint({.5f, .5f});
    auto panel = ui::makePanel(size, {16, 30, 60}, 170);
    panel->setPosition({size.width / 2, size.height / 2});
    addChild(panel);
    m_scroll = ScrollLayer::create({size.width - 8, size.height - 8}, true, true);
    m_scroll->setPosition({4, 4});
    m_scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(3));
    addChild(m_scroll, 2);
    return true;
}

void GdpsList::setRows(std::vector<GdpsRow> rows) {
    ui::clearScroll(m_scroll);
    float rowWidth = m_size.width - 14;
    for (size_t r = 0; r < rows.size(); ++r) {
        auto const& row = rows[r];
        auto node = CCNode::create();
        node->setContentSize({rowWidth, 26});
        node->setAnchorPoint({.5f, .5f});
        float buttonsWidth = row.buttons.size() * 60.f;
        auto label = ui::makeLabel(row.text, "chatFont.fnt", .5f, row.color);
        label->setAnchorPoint({0, .5f});
        label->setPosition({6, 13});
        label->limitLabelWidth(rowWidth - buttonsWidth - 12, .5f, .22f);
        node->addChild(label);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        for (size_t b = 0; b < row.buttons.size(); ++b) {
            auto button = ui::makeTextButton(row.buttons[b], 56, danger(row.buttons[b]) ? "GJ_button_06.png" : "GJ_button_04.png",
                                             this, menu_selector(GdpsList::onPress), .26f, 22);
            button->setTag(int(r * 8 + b));
            button->setPosition({rowWidth - buttonsWidth + 30 + b * 60.f, 13});
            menu->addChild(button);
        }
        node->addChild(menu);
        m_scroll->m_contentLayer->addChild(node);
    }
    if (rows.empty()) {
        auto node = CCNode::create();
        node->setContentSize({rowWidth, 26});
        node->setAnchorPoint({.5f, .5f});
        auto label = ui::makeLabel("Nada por aqui.", "chatFont.fnt", .5f, ui::palette::TextSoft);
        label->setPosition({rowWidth / 2, 13});
        node->addChild(label);
        m_scroll->m_contentLayer->addChild(node);
    }
    m_scroll->m_contentLayer->updateLayout();
    m_scroll->scrollToTop();
}

void GdpsList::onPress(CCObject* sender) {
    if (m_pressed) m_pressed(sender->getTag() / 8, sender->getTag() % 8);
}

// ------------------------------------------------------------ main panel
GdpsPopup* GdpsPopup::create() {
    auto p = new GdpsPopup;
    if (p->init()) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool GdpsPopup::init() {
    CCSize size {440, 280};
    if (!Popup::init(size.width, size.height)) return false;
    frame(this, m_mainLayer, m_bgSprite, size);
    setTitle("GDPS GeoBR", "goldFont.fnt", .72f, 15);
    setID("geobr-gdps");
    m_content = CCNode::create();
    m_mainLayer->addChild(m_content, 4);
    m_status = text(m_mainLayer, "", {size.width / 2, 18}, .4f, 410, ui::palette::TextSoft, "chatFont.fnt");
    render();
    refresh();
    return true;
}

void GdpsPopup::status(std::string const& value) { setText(m_status, value, 410, .4f); }

void GdpsPopup::refresh() {
    if (!gdps::loggedIn()) return;
    status("Carregando conta...");
    m_me.post("me", matjson::Value::object(), [this](gdps::Result r) {
        if (!r.ok) { status(r.error); return; }
        m_info = r.value;
        status("");
        render();
    });
}

void GdpsPopup::render() {
    m_content->removeAllChildren();
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_content->addChild(menu, 5);
    auto row = [&](std::vector<std::tuple<char const*, char const*, SEL_MenuHandler>> const& buttons, float y) {
        float width = buttons.size() > 3 ? 98.f : 128.f;
        for (size_t i = 0; i < buttons.size(); ++i) {
            auto [label, bg, handler] = buttons[i];
            auto b = ui::makeTextButton(label, int(width), bg, this, handler, .32f, 30);
            b->setPosition({220.f + (float(i) - (buttons.size() - 1) / 2.f) * (width + 8.f), y});
            menu->addChild(b);
        }
    };
    if (!gdps::coreInstalled()) {
        text(m_content, "Para usar o GDPS instale tambem o mod GeoBR GDPS (geobr.gdps.geode).", {220, 230}, .42f, 410, {255, 190, 120}, "chatFont.fnt");
        text(m_content, "Ele separa o save e a conta do GDPS do seu GD oficial.", {220, 212}, .4f, 410, ui::palette::TextSoft, "chatFont.fnt");
        row({{"Musicas", "GJ_button_02.png", menu_selector(GdpsPopup::onMusic)}}, 160);
        return;
    }
    bool on = gdps::enabled();
    text(m_content, on ? "Servidor: GDPS GeoBR" : "Servidor: GD Oficial", {150, 236}, .5f, 260, on ? ccColor3B{120, 240, 140} : ccColor3B{255, 200, 120});
    auto sw = ui::makeTextButton("Trocar servidor", 120, "GJ_button_01.png", this, menu_selector(GdpsPopup::onSwitch), .3f, 26);
    sw->setPosition({352, 236});
    menu->addChild(sw);

    if (!on) {
        text(m_content, "No GD Oficial seu save e sua conta de sempre continuam iguais.", {220, 206}, .42f, 410, ui::palette::TextSoft, "chatFont.fnt");
        text(m_content, "O GDPS tem save e conta proprios: troque o servidor para entrar.", {220, 188}, .42f, 410, ui::palette::TextSoft, "chatFont.fnt");
    } else if (gdps::loggedIn()) {
        auto account = m_info.get("account") ? m_info["account"] : matjson::Value::object();
        text(m_content, fmt::format("Conta GDPS: {}  (ID {})", gdps::username(), gdps::accountID()), {220, 208}, .36f, 400, ui::palette::TextGold);
        auto roles = items(m_info, "roles");
        std::string names;
        for (auto const& r : roles) names += (names.empty() ? "" : ", ") + jsonutil::stringOr(r, "name");
        text(m_content, roles.empty() ? "Sem cargo" : names, {220, 188}, .45f, 400,
             roles.empty() ? ui::palette::TextSoft : rgb(jsonutil::integerOr(roles.front(), "color", 0xffffff)), "chatFont.fnt");
        if (jsonutil::boolOr(account, "lb_banned", false))
            text(m_content, "Voce esta fora do ranking por decisao da moderacao.", {220, 172}, .4f, 400, {255, 120, 120}, "chatFont.fnt");
    } else {
        text(m_content, "Sem conta GDPS neste save.", {220, 208}, .45f, 400, ui::palette::TextSoft, "chatFont.fnt");
        text(m_content, "Abra Conta do GD: Registrar (so nome e senha) ou Entrar.", {220, 190}, .42f, 400, ui::palette::TextSoft, "chatFont.fnt");
    }

    row({{"Musicas", "GJ_button_02.png", menu_selector(GdpsPopup::onMusic)},
         {"Conta do GD", "GJ_button_04.png", menu_selector(GdpsPopup::onAccount)}}, 140);
    if (!on) return;
    std::vector<std::tuple<char const*, char const*, SEL_MenuHandler>> second;
    if (gdps::loggedIn()) {
        second.push_back({"Codigo de recuperacao", "GJ_button_04.png", menu_selector(GdpsPopup::onRecoveryCode)});
        second.push_back({"Trocar senha", "GJ_button_04.png", menu_selector(GdpsPopup::onPassword)});
        second.push_back({"Mudar nome", "GJ_button_04.png", menu_selector(GdpsPopup::onRename)});
    } else {
        second.push_back({"Esqueci a senha", "GJ_button_04.png", menu_selector(GdpsPopup::onResetPassword)});
    }
    auto permissions = items(m_info, "permissions");
    bool panel = jsonutil::boolOr(m_info, "owner", false) || std::any_of(permissions.begin(), permissions.end(), [](auto const& p) { return p.asString().unwrapOr("") == "admin.panel"; });
    if (panel) second.push_back({"Painel ADM", "GJ_button_05.png", menu_selector(GdpsPopup::onAdmin)});
    if (gdps::loggedIn() && jsonutil::boolOr(m_info, "owner_available", false)) second.push_back({"Virar Owner", "GJ_button_05.png", menu_selector(GdpsPopup::onOwner)});
    row(second, 96);
    text(m_content, "Save/Load da Conta do GD guarda o progresso do GDPS na nuvem GeoBR.", {220, 56}, .38f, 410, ui::palette::TextSoft, "chatFont.fnt");
}

void GdpsPopup::onSwitch(CCObject*) { if (auto p = ServerSwitchPopup::create()) p->show(); }

void GdpsPopup::onPassword(CCObject*) {
    auto prompt = GdpsPrompt::create("Trocar senha", "Vale para entrar pela Conta do GD a partir de agora.",
        {{"Nova senha", "", CommonFilter::Any, 20, true}, {"Repita a senha", "", CommonFilter::Any, 20, true}}, {"Trocar", "Cancelar"},
        [this](int button, auto const& v) {
            if (button != 0) return;
            if (v[0] != v[1]) { status("As senhas nao sao iguais."); return; }
            status("Trocando a senha...");
            m_action.post("password", matjson::makeObject({{"password", v[0]}}), [this](gdps::Result r) {
                if (!r.ok) { status(r.error); return; }
                if (auto am = GJAccountManager::sharedState()) am->m_GJP2 = jsonutil::stringOr(r.value, "gjp2");
                GameManager::sharedState()->save();
                status("Senha trocada.");
            });
        });
    if (prompt) prompt->show();
}

// Own account name (once a day). GD keeps the name for its login, so the
// account manager and the player name are updated and saved too.
void GdpsPopup::onRename(CCObject*) {
    auto prompt = GdpsPrompt::create("Mudar nome", "3 a 20 letras ou numeros. Uma vez por dia.",
        {{"Novo nome", gdps::username(), CommonFilter::Alphanumeric, 20}}, {"Mudar", "Cancelar"},
        [this](int button, auto const& v) {
            if (button != 0) return;
            status("Mudando o nome...");
            m_action.post("account/rename", matjson::makeObject({{"userName", v[0]}}), [this](gdps::Result r) {
                if (!r.ok) { status(r.error); return; }
                auto name = jsonutil::stringOr(r.value, "username");
                if (auto am = GJAccountManager::sharedState()) am->m_username = name;
                GameManager::sharedState()->m_playerName = name;
                GameManager::sharedState()->save();
                status("Nome trocado para " + name + ".");
                gdps::refreshMe();
            });
        });
    if (prompt) prompt->show();
}

void GdpsPopup::onMusic(CCObject*) { if (auto p = MusicLibraryPopup::create()) p->show(); }
void GdpsPopup::onAccount(CCObject*) { if (auto l = AccountLayer::create()) l->showLayer(false); }
void GdpsPopup::onAdmin(CCObject*) { if (auto p = GdpsAdminPopup::create(m_info)) p->show(); }

void GdpsPopup::onRecoveryCode(CCObject*) {
    auto prompt = GdpsPrompt::create("Codigo de recuperacao", "Gera um codigo novo (o anterior deixa de valer).", {}, {"Gerar", "Cancelar"},
        [this](int button, auto const&) {
            if (button != 0) return;
            status("Gerando codigo...");
            m_action.post("recovery/new", matjson::Value::object(), [this](gdps::Result r) {
                if (!r.ok) { status(r.error); return; }
                auto code = jsonutil::stringOr(r.value, "code");
                utils::clipboard::write(code);
                status("Codigo copiado. Guarde em lugar seguro.");
                if (auto p = GdpsPrompt::create("Seu codigo", "Use em 'Esqueci a senha'. Ja foi copiado.", {{"Codigo", code}}, {"OK"}, nullptr)) p->show();
            });
        });
    if (prompt) prompt->show();
}

void GdpsPopup::onResetPassword(CCObject*) {
    auto prompt = GdpsPrompt::create("Nova senha", "Use o codigo de recuperacao da sua conta.",
        {{"Usuario", gdps::username(), CommonFilter::Alphanumeric, 20}, {"Codigo", "", CommonFilter::Any, 19}, {"Nova senha", "", CommonFilter::Any, 20, true}},
        {"Trocar", "Cancelar"}, [this](int button, auto const& v) {
            if (button != 0) return;
            status("Trocando a senha...");
            m_action.post("recovery/reset", matjson::makeObject({{"userName", v[0]}, {"code", v[1]}, {"password", v[2]}}), [this](gdps::Result r) {
                status(r.ok ? "Senha trocada. Entre de novo pela Conta do GD." : r.error);
            }, false);
        });
    if (prompt) prompt->show();
}

void GdpsPopup::onOwner(CCObject*) {
    auto prompt = GdpsPrompt::create("Virar Owner", "So funciona uma vez, com a senha ADM do GeoBR.", {{"Senha ADM", "", CommonFilter::Any, 64, true}},
        {"Confirmar", "Cancelar"}, [this](int button, auto const& v) {
            if (button != 0) return;
            status("Verificando...");
            m_action.post("owner/claim", matjson::makeObject({{"adminPassword", v[0]}}), [this](gdps::Result r) {
                status(r.ok ? "Agora voce e o Owner do GDPS." : r.error);
                if (r.ok) refresh();
            });
        });
    if (prompt) prompt->show();
}

// ---------------------------------------------------------- music library
MusicLibraryPopup* MusicLibraryPopup::create() {
    auto p = new MusicLibraryPopup;
    if (p->init()) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool MusicLibraryPopup::init() {
    CCSize size {460, 300};
    if (!Popup::init(size.width, size.height)) return false;
    frame(this, m_mainLayer, m_bgSprite, size, "neon");
    setTitle("Musicas GeoBR", "goldFont.fnt", .7f, 15);
    m_search = TextInput::create(190, "Nome, artista ou ID");
    m_search->setMaxCharCount(60);
    m_search->setPosition({112, 252});
    m_mainLayer->addChild(m_search, 4);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 5);
    auto add = [&](char const* label, char const* bg, SEL_MenuHandler h, float x, int w) {
        auto b = ui::makeTextButton(label, w, bg, this, h, .3f, 26);
        b->setPosition({x, 252});
        menu->addChild(b);
    };
    add("Buscar", "GJ_button_01.png", menu_selector(MusicLibraryPopup::onSearch), 248, 64);
    add("Minhas", "GJ_button_04.png", menu_selector(MusicLibraryPopup::onMine), 318, 64);
    add("Enviar MP3", "GJ_button_05.png", menu_selector(MusicLibraryPopup::onUpload), 400, 84);
    m_list = GdpsList::create({430, 196}, [this](int row, int) { useSong(row); });
    m_list->setPosition({size.width / 2, 138});
    m_mainLayer->addChild(m_list, 3);
    m_status = text(m_mainLayer, "", {size.width / 2, 22}, .38f, 430, ui::palette::TextSoft, "chatFont.fnt");
    fetch();
    return true;
}

void MusicLibraryPopup::status(std::string const& value) { setText(m_status, value, 430, .38f); }

void MusicLibraryPopup::fetch() {
    status("Carregando...");
    auto done = [this](gdps::Result r) {
        if (!r.ok) { status(r.error); return; }
        m_songs = r.value["songs"];
        std::vector<GdpsRow> rows;
        auto mdm = MusicDownloadManager::sharedState();
        for (auto const& s : items(r.value, "songs")) {
            auto id = jsonutil::integerOr(s, "id");
            auto st = jsonutil::stringOr(s, "status");
            std::string state = m_mine ? (st == "approved" ? "  [aprovada]" : st == "pending" ? "  [aguardando]" : "  [" + st + "]")
                                       : (mdm && mdm->isSongDownloaded(int(id)) ? "  [baixada]" : "");
            rows.push_back({fmt::format("{}  {} - {}  ({:.1f} MB){}", id, jsonutil::stringOr(s, "name"), jsonutil::stringOr(s, "artist"),
                                        s.get("size_mb") ? s["size_mb"].asDouble().unwrapOr(0) : 0.0, state),
                            st == "approved" ? ccColor3B{255, 255, 255} : ccColor3B{190, 200, 220},
                            st == "approved" ? std::vector<std::string>{"Usar"} : std::vector<std::string>{}});
        }
        m_list->setRows(std::move(rows));
        status(m_mine ? "Suas musicas e o status da moderacao." : "Usar: baixa a musica e copia o ID para o editor (Custom Song).");
    };
    if (m_mine) m_fetch.post("songs/mine", matjson::Value::object(), done);
    else m_fetch.post("songs/search", matjson::makeObject({{"q", std::string(m_search->getString())}}), done, false);
}

void MusicLibraryPopup::onSearch(CCObject*) { m_mine = false; fetch(); }
void MusicLibraryPopup::onMine(CCObject*) { m_mine = true; fetch(); }

void MusicLibraryPopup::useSong(int row) {
    auto songs = items(matjson::makeObject({{"v", m_songs}}), "v");
    if (row < 0 || row >= int(songs.size())) return;
    auto id = int(jsonutil::integerOr(songs[row], "id"));
    utils::clipboard::write(std::to_string(id));
    if (auto mdm = MusicDownloadManager::sharedState()) mdm->getSongInfo(id, true);
    status(fmt::format("Musica {} baixando. ID copiado: cole em Custom Song no editor.", id));
}

void MusicLibraryPopup::onUpload(CCObject*) {
    if (m_busy) return;
    if (!gdps::loggedIn()) { status("Entre na sua conta GeoBR para enviar musicas."); return; }
    auto prompt = GdpsPrompt::create("Enviar MP3", "MP3 de ate 15 MB. Musicas novas passam pela moderacao.",
        {{"Nome", "", CommonFilter::Any, 60}, {"Artista", "", CommonFilter::Any, 40}}, {"Escolher arquivo", "Da pasta", "Cancelar"},
        [this](int button, auto const& v) {
            if (button != 0 && button != 1) return;
            if (v[0].empty() || v[1].empty()) { status("Informe nome e artista."); return; }
            if (button == 0) pickAndSend(v[0], v[1]);
            else fromFolder(v[0], v[1]);
        });
    if (prompt) prompt->show();
}

void MusicLibraryPopup::pickAndSend(std::string name, std::string artist) {
    m_busy = true;
    status("Escolha o MP3...");
    file::FilePickOptions options;
    options.filters = {{"MP3", {"*.mp3"}}};
    m_picker.spawn("GeoBR - escolher MP3", file::pick(file::PickMode::OpenFile, options), [this, name, artist](file::PickResult result) {
        if (result.isErr()) {
            m_busy = false;
            log::warn("Seletor de arquivo: {}", result.unwrapErr());
            status(fmt::format("Seletor nao abriu ({}). Use \"Da pasta\".", result.unwrapErr()));
            return;
        }
        if (!result.unwrap()) { m_busy = false; status("Envio cancelado."); return; }
        sendFile(*result.unwrap(), name, artist);
    });
}

std::filesystem::path MusicLibraryPopup::musicFolder() { return Mod::get()->getConfigDir() / "musicas"; }

// Without the system file picker (some phones / launchers): the newest MP3 in
// the mod's "musicas" folder (on Android it is inside Android/media, reachable
// with any file manager).
void MusicLibraryPopup::fromFolder(std::string name, std::string artist) {
    auto folder = musicFolder();
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    std::filesystem::path newest;
    std::filesystem::file_time_type newestTime{};
    for (auto const& entry : std::filesystem::directory_iterator(folder, error)) {
        std::error_code e;
        if (!entry.is_regular_file(e)) continue;
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".mp3") continue;
        auto time = entry.last_write_time(e);
        if (newest.empty() || time > newestTime) { newest = entry.path(); newestTime = time; }
    }
    auto where = utils::string::pathToString(folder);
    if (newest.empty()) {
        utils::clipboard::write(where);
        FLAlertLayer::create(nullptr, "Pasta de musicas",
            fmt::format("Coloque o MP3 nesta pasta (caminho copiado) e toque em <cg>Da pasta</c> de novo:\n\n<cy>{}</c>", where), "OK", nullptr, 400.f)->show();
        status("Nenhum MP3 na pasta de musicas.");
        return;
    }
    auto file = utils::string::pathToString(newest.filename());
    WeakRef<MusicLibraryPopup> weak = this;
    createQuickPopup("Enviar MP3", fmt::format("Enviar <cy>{}</c> como <cg>{}</c> - {}?", file, name, artist), "Cancelar", "Enviar",
        [weak, newest, name, artist](auto, bool send) {
            auto self = weak.lock();
            if (!self || !send) return;
            self->m_busy = true;
            self->sendFile(newest, name, artist);
        });
}

void MusicLibraryPopup::sendFile(std::filesystem::path const& path, std::string name, std::string artist) {
    {
        std::error_code error;
        auto size = std::filesystem::file_size(path, error);
        if (error || size < 1024 || size > 15u * 1024 * 1024) { m_busy = false; status("O MP3 precisa ter entre 1 KB e 15 MB."); return; }
        auto bytes = file::readBinary(path);
        if (bytes.isErr()) { m_busy = false; status("Falha ao ler o arquivo."); return; }
        auto data = std::move(bytes).unwrap();
        status("Preparando envio...");
        m_upload.post("songs/upload", matjson::makeObject({{"name", name}, {"artist", artist}}), [this, data = std::move(data)](gdps::Result r) mutable {
            if (!r.ok) { m_busy = false; status(r.error); return; }
            auto id = jsonutil::integerOr(r.value, "songID");
            auto url = jsonutil::stringOr(r.value, "uploadUrl");
            status(fmt::format("Enviando musica {}...", id));
            WebRequest put;
            put.header("Content-Type", "audio/mpeg").timeout(std::chrono::seconds{180}).certVerification(true).followRedirects(false);
            put.body(std::move(data));
            m_put.spawn("GeoBR - enviar MP3", put.send("PUT", url), [this, id](WebResponse response) {
                if (!response.ok()) { m_busy = false; status(fmt::format("Falha no envio (HTTP {}).", response.code())); return; }
                status("Conferindo o arquivo...");
                m_upload.post("songs/confirm", matjson::makeObject({{"songID", id}}), [this, id](gdps::Result c) {
                    m_busy = false;
                    if (!c.ok) { status(c.error); return; }
                    bool approved = jsonutil::stringOr(c.value, "status") == "approved";
                    utils::clipboard::write(std::to_string(id));
                    status(approved ? fmt::format("Musica {} publicada! ID copiado.", id)
                                    : fmt::format("Musica {} enviada. Fica disponivel quando a moderacao aprovar.", id));
                    m_mine = true;
                    fetch();
                });
            });
        });
    }
}

} // namespace geobr
