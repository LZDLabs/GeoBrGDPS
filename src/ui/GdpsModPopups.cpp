// Server switch and the in-game GDPS moderation popups (level page and
// profile). Every action goes through the admin API, which checks the
// permission again on the server.
#include "GdpsPopups.hpp"
#include "GdpsRatePopup.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../models/JsonModelUtils.hpp"

using namespace geode::prelude;

namespace geobr {
namespace {

void chrome(CCNode* main, CCNode* bg, CCSize size, char const* style) {
    visual::fitPopup(main, size);
    visual::stylePopup(main, bg, size, {44, 38, 86});
    auto backdrop = visual::theme({size.width - 14, size.height - 14}, style);
    backdrop->setPosition({7, 7});
    main->addChild(backdrop, -1);
}

CCLabelBMFont* label(CCNode* parent, std::string const& value, CCPoint pos, float scale, float width, ccColor3B color = {255, 255, 255}, char const* font = "chatFont.fnt") {
    auto l = ui::makeLabel(value, font, scale, color);
    l->setPosition(pos);
    l->limitLabelWidth(width, scale, .15f);
    parent->addChild(l, 3);
    return l;
}

void setLabel(CCLabelBMFont* l, std::string const& value, float width, float scale) {
    if (!l) return;
    l->setString(value.c_str());
    l->limitLabelWidth(width, scale, .15f);
}

int toInt(std::string const& s, int fallback = 0) {
    try { return s.empty() ? fallback : std::stoi(s); } catch (...) { return fallback; }
}

// Buttons laid out in rows of `perRow`, centred.
struct Grid {
    CCMenu* menu;
    CCObject* target;
    SEL_MenuHandler handler;
    float centerX, top, width, height, gap;
    int perRow;
    int count = 0;
    void add(std::string const& text, char const* bg, int tag) {
        auto b = ui::makeTextButton(text, int(width), bg, target, handler, .3f, height);
        b->setTag(tag);
        int r = count / perRow, c = count % perRow;
        b->setPosition({centerX + (c - (perRow - 1) / 2.f) * (width + gap), top - r * (height + gap)});
        menu->addChild(b);
        ++count;
    }
};

} // namespace

// ------------------------------------------------------------- server switch
ServerSwitchPopup* ServerSwitchPopup::create() {
    auto p = new ServerSwitchPopup;
    if (p->init()) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool ServerSwitchPopup::init() {
    CCSize size {400, 250};
    if (!Popup::init(size.width, size.height)) return false;
    chrome(m_mainLayer, m_bgSprite, size, "cosmos");
    setTitle("Servidor", "goldFont.fnt", .72f, 15);
    setID("geobr-server-switch");
    if (!gdps::coreInstalled()) {
        label(m_mainLayer, "Instale o mod GeoBR GDPS (geobr.gdps.geode) para trocar de servidor.", {200, 140}, .45f, 370, {255, 190, 120});
        return true;
    }
    bool on = gdps::enabled();
    label(m_mainLayer, on ? "Agora: GDPS GeoBR" : "Agora: GD Oficial", {200, 202}, .5f, 360, on ? ccColor3B{120, 240, 140} : ccColor3B{255, 200, 120}, "bigFont.fnt");
    label(m_mainLayer, "Cada servidor tem o seu proprio save e a sua propria conta.", {200, 176}, .42f, 370, ui::palette::TextSoft);
    label(m_mainLayer, "Nada do GD Oficial e apagado. O jogo salva e reinicia ao trocar.", {200, 160}, .42f, 370, ui::palette::TextSoft);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 5);
    char const* names[] = {"GD Oficial", "GDPS GeoBR"};
    for (int i = 0; i < 2; ++i) {
        bool current = (i == 1) == on;
        auto b = ui::makeTextButton(current ? std::string(names[i]) + " (atual)" : names[i], 160, current ? "GJ_button_04.png" : (i ? "GJ_button_01.png" : "GJ_button_02.png"),
                                    this, menu_selector(ServerSwitchPopup::onPick), .34f, 36);
        b->setTag(i);
        b->setPosition({110.f + i * 180.f, 100});
        if (current) b->setEnabled(false);
        menu->addChild(b);
    }
    label(m_mainLayer, "Tambem da para trocar em Geode > GeoBR GDPS > Servidor.", {200, 40}, .38f, 370, ui::palette::TextSoft);
    return true;
}

void ServerSwitchPopup::onPick(CCObject* sender) {
    bool gdps = sender->getTag() == 1;
    auto prompt = GdpsPrompt::create(gdps ? "Ir para o GDPS GeoBR" : "Voltar ao GD Oficial",
        gdps ? "O jogo salva e reinicia no GDPS (save e conta do GDPS)." : "O jogo salva e reinicia no GD Oficial (seu save de sempre).",
        {}, {"Reiniciar", "Cancelar"}, [gdps](int button, auto const&) {
            if (button == 0) gdps::switchServer(gdps);
        });
    if (prompt) prompt->show();
}

// ---------------------------------------------------------------- level mod
namespace {
enum LevelAction { Rate, Unrate, Featured, Epic, Legendary, Mythic, NoTier, Coins, Daily, Weekly, Event, DemonList, Block, Unblock, Delete };
}

LevelModPopup* LevelModPopup::create(int levelID, std::string name) {
    auto p = new LevelModPopup;
    if (p->init(levelID, std::move(name))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool LevelModPopup::init(int levelID, std::string name) {
    CCSize size {440, 280};
    if (!Popup::init(size.width, size.height)) return false;
    chrome(m_mainLayer, m_bgSprite, size, "eclipse");
    m_level = levelID;
    m_name = std::move(name);
    setTitle("Moderar level", "goldFont.fnt", .62f, 14);
    setID("geobr-gdps-level-mod");
    label(m_mainLayer, fmt::format("{}  (ID {})", m_name, m_level), {220, 240}, .5f, 400, ui::palette::TextGold);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 5);
    Grid grid {menu, this, menu_selector(LevelModPopup::onAction), 220, 206, 98, 26, 6, 4};
    // Rating itself uses GD's own star button (RobTop's moderator popup).
    if (gdps::can("levels.rate")) { grid.add("Rate", "GJ_button_01.png", Rate); grid.add("Tirar rate", "GJ_button_04.png", Unrate); }
    if (gdps::can("events.manage")) { grid.add("Daily", "GJ_button_05.png", Daily); grid.add("Weekly", "GJ_button_05.png", Weekly); grid.add("Evento", "GJ_button_05.png", Event); }
    if (gdps::can("levels.block")) { grid.add("Bloquear", "GJ_button_06.png", Block); grid.add("Desbloquear", "GJ_button_04.png", Unblock); }
    if (gdps::can("levels.delete")) grid.add("Apagar", "GJ_button_06.png", Delete);
    if (!grid.count) label(m_mainLayer, "Seu cargo nao tem acoes de level.", {220, 150}, .45f, 400, ui::palette::TextSoft);
    m_status = label(m_mainLayer, "Daily/Weekly entram na fila. Tambem: /daily, /weekly, /event nos comentarios.", {220, 22}, .38f, 410, ui::palette::TextSoft);
    return true;
}

void LevelModPopup::status(std::string const& text) { setLabel(m_status, text, 410, .38f); }

void LevelModPopup::act(std::string const& action, matjson::Value fields, std::string const& done) {
    fields["levelID"] = m_level;
    status("Enviando...");
    m_action.admin(action, std::move(fields), [this, done](gdps::Result r) { status(r.ok ? done : r.error); });
}

void LevelModPopup::onAction(CCObject* sender) {
    int a = sender->getTag();
    auto confirmThen = [this](std::string title, std::string body, std::vector<GdpsPrompt::Field> fields, std::function<void(std::vector<std::string> const&)> go) {
        if (auto p = GdpsPrompt::create(title, body, std::move(fields), {"Confirmar", "Cancelar"}, [go](int b, auto const& v) { if (b == 0) go(v); })) p->show();
    };
    switch (a) {
        case Rate:
            if (auto p = GdpsRatePopup::create(m_level, m_name)) p->show();
            break;
        case Unrate: act("levels.rate", matjson::makeObject({{"stars", 0}}), "Rate removido."); break;
        case Coins: confirmThen("Moedas", "Verificar as moedas mantem o rate atual.", {{"Estrelas atuais", "", CommonFilter::Uint, 2}},
                [this](auto const& v) { act("levels.rate", matjson::makeObject({{"stars", toInt(v[0])}, {"coins", true}}), "Moedas verificadas."); });
            break;
        case Featured: act("levels.feature", matjson::makeObject({{"feature", true}, {"epic", 0}}), "Featured."); break;
        case Epic: act("levels.feature", matjson::makeObject({{"feature", true}, {"epic", 1}}), "Epic."); break;
        case Legendary: act("levels.feature", matjson::makeObject({{"feature", true}, {"epic", 2}}), "Legendary."); break;
        case Mythic: act("levels.feature", matjson::makeObject({{"feature", true}, {"epic", 3}}), "Mythic."); break;
        case NoTier: act("levels.feature", matjson::makeObject({{"feature", false}, {"epic", 0}}), "Destaque removido."); break;
        case Daily: case Weekly: {
            // A queue: today's (this week's) slot if free, else the next free one.
            int kind = a - Daily;
            status("Enviando...");
            m_action.admin("events.set", matjson::makeObject({{"levelID", m_level}, {"kind", kind}}), [this, kind](gdps::Result r) {
                if (!r.ok) { status(r.error); return; }
                auto day = jsonutil::stringOr(r.value, "starts_at").substr(0, 10);
                status(jsonutil::boolOr(r.value, "queued", false) ? fmt::format("Na fila: entra em {}.", day)
                                                                   : (kind == 0 ? "Daily de hoje definido." : "Weekly desta semana definido."));
            });
            break;
        }
        case Event:
            confirmThen("Evento", "Fica ate voce definir outro level. Ex.: 500 diamantes 1000 orbs 1 chave",
                {{"Recompensas", "500 diamantes 1000 orbs", CommonFilter::Any, 80}},
                [this](auto const& v) { act("events.set", matjson::makeObject({{"kind", 2}, {"rewards", v[0]}}), "Evento definido."); });
            break;
        case DemonList:
            confirmThen("Demon List", "Posicao na lista (os outros descem).", {{"Posicao", "1", CommonFilter::Uint, 4}, {"Verificador", "", CommonFilter::Any, 30}},
                [this](auto const& v) { act("demonlist.set", matjson::makeObject({{"position", toInt(v[0], 1)}, {"verifier", v[1]}}), "Demon List atualizada."); });
            break;
        case Block:
            confirmThen("Bloquear level", "Some das buscas e dos downloads.", {{"Motivo", "", CommonFilter::Any, 120}},
                [this](auto const& v) { act("levels.block", matjson::makeObject({{"reason", v[0]}}), "Level bloqueado."); });
            break;
        case Unblock: act("levels.unblock", matjson::Value::object(), "Level desbloqueado."); break;
        case Delete:
            confirmThen("Apagar level", "Pode ser restaurado no Painel ADM.", {{"Motivo", "", CommonFilter::Any, 120}},
                [this](auto const& v) { act("levels.delete", matjson::makeObject({{"reason", v[0]}}), "Level apagado."); });
            break;
    }
}

// ----------------------------------------------------------------- user mod
namespace {
enum UserAction { Ban, Unban, Mute, Unmute, LbBan, LbUnban, Reset, History, Role };
}

UserModPopup* UserModPopup::create(int accountID, std::string name) {
    auto p = new UserModPopup;
    if (p->init(accountID, std::move(name))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool UserModPopup::init(int accountID, std::string name) {
    CCSize size {440, 290};
    if (!Popup::init(size.width, size.height)) return false;
    chrome(m_mainLayer, m_bgSprite, size, "eclipse");
    m_account = accountID;
    m_name = std::move(name);
    setTitle("Moderar jogador", "goldFont.fnt", .62f, 14);
    setID("geobr-gdps-user-mod");
    label(m_mainLayer, fmt::format("{}  (ID {})", m_name, m_account), {220, 250}, .5f, 400, ui::palette::TextGold);
    m_state = label(m_mainLayer, "", {220, 232}, .4f, 410, ui::palette::TextSoft);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 5);
    Grid grid {menu, this, menu_selector(UserModPopup::onAction), 220, 206, 98, 26, 6, 4};
    if (gdps::can("accounts.ban")) { grid.add("Banir", "GJ_button_06.png", Ban); grid.add("Desbanir", "GJ_button_04.png", Unban); }
    if (gdps::can("comments.moderate")) { grid.add("Mutar", "GJ_button_06.png", Mute); grid.add("Desmutar", "GJ_button_04.png", Unmute); }
    if (gdps::can("leaderboard.moderate")) {
        grid.add("Tirar do ranking", "GJ_button_06.png", LbBan); grid.add("Voltar ao ranking", "GJ_button_04.png", LbUnban);
        grid.add("Zerar stats", "GJ_button_06.png", Reset); grid.add("Historico", "GJ_button_02.png", History);
    }
    if (gdps::can("roles.manage")) grid.add("Cargo", "GJ_button_05.png", Role);
    float listTop = 206 - ((grid.count + 3) / 4) * 32.f;
    m_list = GdpsList::create({410, std::max(40.f, listTop - 30.f - 26.f)}, nullptr);
    m_list->setPosition({220, (listTop - 14.f + 40.f) / 2.f});
    m_list->setVisible(false);
    m_mainLayer->addChild(m_list, 3);
    m_status = label(m_mainLayer, "Tambem por comentario: !ban NOME, !lbban NOME, !mute NOME horas...", {220, 22}, .38f, 410, ui::palette::TextSoft);
    refresh();
    return true;
}

void UserModPopup::status(std::string const& text) { setLabel(m_status, text, 410, .38f); }

void UserModPopup::refresh() {
    if (!gdps::can("accounts.view")) return;
    m_fetch.admin("accounts.get", matjson::makeObject({{"target", m_account}}), [this](gdps::Result r) {
        if (!r.ok) { setLabel(m_state, r.error, 410, .4f); return; }
        auto a = r.value["account"];
        std::string state = fmt::format("{} estrelas, {} demons, {} levels", jsonutil::integerOr(a, "stars"), jsonutil::integerOr(a, "demons"), jsonutil::integerOr(a, "levels"));
        if (jsonutil::boolOr(a, "is_banned", false)) state += "  |  BANIDO";
        if (jsonutil::boolOr(a, "lb_banned", false)) state += "  |  FORA DO RANKING";
        setLabel(m_state, state, 410, .4f);
    });
}

void UserModPopup::act(std::string const& action, matjson::Value fields, std::string const& done) {
    fields["target"] = m_account;
    status("Enviando...");
    m_action.admin(action, std::move(fields), [this, done](gdps::Result r) {
        status(r.ok ? done : r.error);
        if (r.ok) refresh();
    });
}

void UserModPopup::onAction(CCObject* sender) {
    auto confirmThen = [](std::string title, std::string body, std::vector<GdpsPrompt::Field> fields, std::function<void(std::vector<std::string> const&)> go) {
        if (auto p = GdpsPrompt::create(title, body, std::move(fields), {"Confirmar", "Cancelar"}, [go](int b, auto const& v) { if (b == 0) go(v); })) p->show();
    };
    switch (sender->getTag()) {
        case Ban:
            confirmThen("Banir " + m_name, "Dias = 0: permanente.", {{"Motivo", "", CommonFilter::Any, 120}, {"Dias", "0", CommonFilter::Uint, 4}},
                [this](auto const& v) { act("accounts.ban", matjson::makeObject({{"reason", v[0]}, {"days", toInt(v[1])}}), "Jogador banido."); });
            break;
        case Unban: act("accounts.unban", matjson::Value::object(), "Banimento removido."); break;
        case Mute:
            confirmThen("Mutar " + m_name, "Sem comentarios durante esse tempo.", {{"Horas", "24", CommonFilter::Uint, 5}, {"Motivo", "", CommonFilter::Any, 100}},
                [this](auto const& v) { act("accounts.mute", matjson::makeObject({{"hours", toInt(v[0], 24)}, {"reason", v[1]}}), "Comentarios bloqueados."); });
            break;
        case Unmute: act("accounts.unmute", matjson::Value::object(), "Comentarios liberados."); break;
        case LbBan:
            confirmThen("Tirar do ranking", "So o leaderboard: a conta continua jogando.", {{"Motivo", "", CommonFilter::Any, 120}},
                [this](auto const& v) { act("lb.ban", matjson::makeObject({{"reason", v[0]}}), "Fora do ranking."); });
            break;
        case LbUnban: act("lb.unban", matjson::Value::object(), "De volta ao ranking."); break;
        case Reset:
            confirmThen("Zerar estatisticas", "Estrelas, luas, demons e moedas vao a 0 (o historico fica).", {},
                [this](auto const&) { act("lb.reset", matjson::Value::object(), "Estatisticas zeradas."); });
            break;
        case History:
            status("Carregando historico...");
            m_fetch.admin("lb.history", matjson::makeObject({{"target", m_account}}), [this](gdps::Result r) {
                if (!r.ok) { status(r.error); return; }
                std::vector<GdpsRow> rows;
                auto h = r.value["history"];
                if (h.isArray()) for (auto const& e : h.asArray().unwrap()) {
                    bool flagged = jsonutil::boolOr(e, "flagged", false);
                    rows.push_back({fmt::format("{}  {} estrelas  {} demons  {} luas{}", ui::slice(jsonutil::stringOr(e, "at"), 0, 16), jsonutil::integerOr(e, "stars"),
                                                jsonutil::integerOr(e, "demons"), jsonutil::integerOr(e, "moons"), flagged ? "  [SUSPEITO]" : ""),
                                    flagged ? ccColor3B{255, 130, 130} : ccColor3B{255, 255, 255}, {}});
                }
                m_list->setRows(std::move(rows));
                m_list->setVisible(true);
                status("Historico de estatisticas (mais recente primeiro).");
            });
            break;
        case Role:
            confirmThen("Cargo de " + m_name, "ID do cargo (veja em Painel ADM > Cargos). Dar = 1, tirar = 0.",
                {{"ID do cargo", "", CommonFilter::Uint, 6}, {"Dar (1) / Tirar (0)", "1", CommonFilter::Uint, 1}},
                [this](auto const& v) {
                    bool give = v[1] != "0";
                    act(give ? "roles.grant" : "roles.revoke", matjson::makeObject({{"role_id", toInt(v[0])}}), give ? "Cargo dado." : "Cargo retirado.");
                });
            break;
    }
}

} // namespace geobr
