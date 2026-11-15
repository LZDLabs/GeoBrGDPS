// GDPS admin panel. The tabs shown follow the permissions the server returned
// for this account; every action is checked again on the server.
#include "GdpsPopups.hpp"
#include "GdpsRatePopup.hpp"
#include "AdminPanel.hpp"
#include <Geode/binding/GJAccountManager.hpp>
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../models/JsonModelUtils.hpp"

#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/utils/base64.hpp>

using namespace geode::prelude;

namespace geobr {
namespace {

constexpr CCSize Size {520, 310};
char const* TabNames[] = {"Resumo", "Jogadores", "Levels", "Coment.", "Eventos", "Demons", "Musicas", "Cargos", "Ranking", "Auditoria", "Packs"};
char const* GauntletNames[] = {"", "Fire", "Ice", "Poison", "Shadow", "Lava", "Bonus", "Chaos", "Demon", "Time", "Crystal", "Magic", "Spike", "Monster", "Doom", "Death"};

std::vector<matjson::Value> list(matjson::Value const& v) {
    std::vector<matjson::Value> out;
    if (v.isArray()) for (auto const& x : v.asArray().unwrap()) out.push_back(x);
    return out;
}
std::string str(matjson::Value const& v, char const* k) {
    auto r = v.get(k);
    if (!r) return {};
    auto x = r.unwrap();
    if (x.isString()) return x.asString().unwrap();
    if (x.isNumber()) return std::to_string(x.asInt().unwrapOr(0));
    if (x.isBool()) return x.asBool().unwrap() ? "sim" : "nao";
    return {};
}
int64_t num(matjson::Value const& v, char const* k) { return jsonutil::integerOr(v, k); }
bool flag(matjson::Value const& v, char const* k) { return jsonutil::boolOr(v, k, false); }
std::string date(matjson::Value const& v, char const* k) { return ui::slice(str(v, k), 0, 16); }
int toInt(std::string const& s, int fallback = 0) {
    try { return s.empty() ? fallback : std::stoi(s); } catch (...) { return fallback; }
}
std::string decodeComment(std::string const& b64) {
    auto r = utils::base64::decodeString(b64, utils::base64::Base64Variant::Url);
    return r.isOk() ? r.unwrap() : b64;
}
size_t countIds(std::string const& ids) {
    if (ids.empty()) return 0;
    return 1 + static_cast<size_t>(std::count(ids.begin(), ids.end(), ','));
}
char const* tierName(int64_t epic) {
    switch (epic) { case 1: return " [Epic]"; case 2: return " [Legendary]"; case 3: return " [Mythic]"; default: return ""; }
}

} // namespace

GdpsAdminPopup* GdpsAdminPopup::create(matjson::Value me) {
    auto p = new GdpsAdminPopup;
    if (p->init(std::move(me))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool GdpsAdminPopup::has(char const* permission) const {
    if (jsonutil::boolOr(m_me, "owner", false)) return true;
    auto perms = m_me.get("permissions");
    if (!perms) return false;
    for (auto const& p : list(perms.unwrap())) if (p.asString().unwrapOr("") == permission) return true;
    return false;
}

bool GdpsAdminPopup::tabAllowed(Tab t) const {
    switch (t) {
        case Stats: return has("stats.view");
        case Players: return has("accounts.view");
        case Levels: return has("levels.rate") || has("levels.feature") || has("levels.delete") || has("levels.block");
        case Comments: return has("comments.moderate");
        case Events: return has("events.manage");
        case Demons: return false; // Demon List removed from the mod
        case Songs: return has("songs.moderate");
        case Roles: return has("admin.panel");
        case Board: return has("leaderboard.moderate");
        case Audit: return has("audit.view");
        case Packs: return has("admin.panel");
        default: return false;
    }
}

bool GdpsAdminPopup::init(matjson::Value me) {
    if (!Popup::init(Size.width, Size.height)) return false;
    visual::fitPopup(m_mainLayer, Size);
    visual::stylePopup(m_mainLayer, m_bgSprite, Size, {70, 40, 40});
    auto backdrop = visual::theme({Size.width - 14, Size.height - 14}, "eclipse");
    backdrop->setPosition({7, 7});
    m_mainLayer->addChild(backdrop, -1);
    setTitle("Painel ADM - GDPS", "goldFont.fnt", .62f, 13);
    setID("geobr-gdps-admin");
    m_me = std::move(me);

    auto tabs = CCMenu::create();
    tabs->setPosition({0, 0});
    m_mainLayer->addChild(tabs, 5);
    for (int i = 0; i < TabCount; ++i) {
        auto b = ui::makeTextButton(TabNames[i], 78, "GJ_button_04.png", this, menu_selector(GdpsAdminPopup::onTab), .24f, 20);
        b->setTag(i);
        b->setPosition({48.f + (i % 6) * 84.8f, i < 6 ? 270.f : 248.f});
        b->setVisible(tabAllowed(Tab(i)));
        tabs->addChild(b);
        m_tabButtons.push_back(b);
    }
    m_tools = CCMenu::create();
    m_tools->setPosition({0, 0});
    m_mainLayer->addChild(m_tools, 5);
    m_search = TextInput::create(170, "Buscar");
    m_search->setMaxCharCount(30);
    m_search->setPosition({102, 220});
    m_mainLayer->addChild(m_search, 5);
    m_list = GdpsList::create({494, 172}, [this](int row, int button) { onRow(row, button); });
    m_list->setPosition({Size.width / 2, 116});
    m_mainLayer->addChild(m_list, 3);
    m_status = ui::makeLabel("", "chatFont.fnt", .4f, ui::palette::TextSoft);
    m_status->setPosition({Size.width / 2, 18});
    m_mainLayer->addChild(m_status, 5);

    // Role names for the "Cargo" action.
    if (has("admin.panel")) m_fetch.admin("roles.list", matjson::Value::object(), [this](gdps::Result r) {
        if (r.ok) m_roles = r.value["roles"];
        for (int i = 0; i < TabCount; ++i) if (tabAllowed(Tab(i))) { selectTab(Tab(i)); return; }
        status("Sua conta nao tem permissoes de painel.");
    });
    return true;
}

void GdpsAdminPopup::status(std::string const& text) {
    m_status->setString(text.c_str());
    m_status->limitLabelWidth(490, .4f, .2f);
}

void GdpsAdminPopup::onTab(CCObject* sender) { selectTab(Tab(sender->getTag())); }

void GdpsAdminPopup::selectTab(Tab tab) {
    m_tab = tab;
    m_sub = 0;
    for (int i = 0; i < TabCount; ++i) m_tabButtons[i]->setOpacity(i == tab ? 255 : 140);
    buildTools();
    load();
}

void GdpsAdminPopup::buildTools() {
    m_tools->removeAllChildren();
    std::vector<std::string> labels;
    bool search = false;
    switch (m_tab) {
        case Stats: labels = jsonutil::boolOr(m_me, "owner", false) ? std::vector<std::string>{"Atualizar", "Painel GeoBR"} : std::vector<std::string>{"Atualizar"}; break;
        case Players: search = true; labels = {"Buscar"}; break;
        case Levels: search = true; labels = {"Buscar", "Denuncias", "Sugeridos", "Listas"}; break;
        case Comments: search = true; labels = {"Buscar"}; break;
        case Events: labels = {"Novo Daily", "Novo Weekly", "Novo Evento"}; break;
        case Demons: labels = {"Adicionar"}; break;
        case Songs: labels = {"Pendentes", "Aprovadas", "Bloqueadas"}; break;
        case Roles: labels = has("roles.manage") ? std::vector<std::string>{"Cargos", "Novo cargo"} : std::vector<std::string>{"Cargos"}; break;
        case Board: search = true; labels = {"Suspeitos", "Scores do level"}; break;
        case Audit: labels = {"Recentes", "Anteriores"}; break;
        case Packs: labels = has("packs.manage") ? std::vector<std::string>{"Map Packs", "Gauntlets", "Novo Pack", "Gauntlet"} : std::vector<std::string>{"Map Packs", "Gauntlets"}; break;
        default: break;
    }
    bool many = labels.size() >= 4;
    float searchWidth = many ? 130.f : 170.f;
    float width = many ? 82.f : 104.f;
    m_search->setVisible(search);
    m_search->setString("");
    m_search->setWidth(searchWidth);
    m_search->setPosition({18.f + searchWidth / 2.f, 220});
    m_search->setPlaceholder(m_tab == Comments || m_tab == Board ? "ID do level" : "Nome ou ID");
    float x = (search ? 30.f + searchWidth : 18.f) + width / 2.f;
    for (size_t i = 0; i < labels.size(); ++i) {
        auto b = ui::makeTextButton(labels[i], int(width), i == 0 ? "GJ_button_01.png" : "GJ_button_04.png", this, menu_selector(GdpsAdminPopup::onTool), .28f, 24);
        b->setTag(int(i));
        b->setPosition({x + i * (width + 6.f), 220});
        m_tools->addChild(b);
    }
}

void GdpsAdminPopup::onTool(CCObject* sender) {
    int tool = sender->getTag();
    auto query = std::string(m_search->getString());
    switch (m_tab) {
        case Players: case Comments: m_sub = 0; m_query = query; break;
        case Levels: m_sub = tool; m_query = query; break;
        case Songs: m_sub = tool; break;
        case Stats:
            if (tool == 1) { openGeoBRPanel(); return; }
            break;
        case Packs:
            if (tool == 2) { editPack(matjson::Value::object()); return; }
            if (tool == 3) { editGauntlet(matjson::Value::object()); return; }
            m_sub = tool; break;
        case Audit: m_sub = tool == 0 ? 0 : m_sub + 1; break;
        case Board:
            if (tool == 1 && query.empty()) { status("Digite o ID do level."); return; }
            m_sub = tool; m_query = query; break;
        case Roles:
            if (tool == 1) {
                if (auto p = GdpsRolePopup::create(matjson::Value::object(), matjson::Value::array(), [this] { load(); })) p->show();
                return;
            }
            m_sub = 0; break;
        case Events: {
            static char const* kinds[] = {"Daily", "Weekly", "Evento"};
            // Daily/Weekly: a queue (one level per day / week). Event: stays until the next one, with rewards.
            std::vector<GdpsPrompt::Field> fields = {{"ID do level", "", CommonFilter::Uint, 10}};
            if (tool == 2) fields.push_back({"Recompensas", "500 diamantes 1000 orbs", CommonFilter::Any, 80});
            auto prompt = GdpsPrompt::create(fmt::format("Novo {}", kinds[tool]),
                tool == 2 ? "Fica ate voce definir outro level." : "Entra na fila: hoje se o dia estiver livre, senao no proximo livre.",
                fields, {"Salvar", "Cancelar"},
                [this, tool](int b, auto const& v) {
                    if (b != 0) return;
                    auto body = matjson::makeObject({{"kind", tool}, {"levelID", toInt(v[0])}});
                    if (tool == 2) body["rewards"] = v[1];
                    act("events.set", body, tool == 2 ? "Evento definido." : "Colocado na fila.");
                });
            if (prompt) prompt->show();
            return;
        }
        case Demons: {
            auto prompt = GdpsPrompt::create("Demon List", "Entra na posicao escolhida; os outros descem.",
                {{"ID do level", "", CommonFilter::Uint, 10}, {"Posicao", "1", CommonFilter::Uint, 4}, {"Verificador", "", CommonFilter::Any, 30}},
                {"Salvar", "Cancelar"}, [this](int b, auto const& v) {
                    if (b != 0) return;
                    act("demonlist.set", matjson::makeObject({{"levelID", toInt(v[0])}, {"position", toInt(v[1], 1)}, {"verifier", v[2]}}), "Demon List atualizada.");
                });
            if (prompt) prompt->show();
            return;
        }
        default: break;
    }
    load();
}

void GdpsAdminPopup::load() {
    status("Carregando...");
    std::string action;
    auto fields = matjson::Value::object();
    switch (m_tab) {
        case Stats: action = "stats"; break;
        case Players: action = "accounts.search"; fields["q"] = m_query; break;
        case Levels: action = m_sub == 1 ? "levels.reports" : m_sub == 2 ? "suggestions.list" : m_sub == 3 ? "lists.search" : "levels.search"; fields["q"] = m_query; break;
        case Packs:
            m_fetch.admin("packs.list", matjson::Value::object(), [this](gdps::Result r) {
                if (!r.ok) { status(r.error); return; }
                showRows(r.value[m_sub == 1 ? "gauntlets" : "packs"]);
            });
            return;
        case Comments: action = "comments.recent"; fields["levelID"] = toInt(m_query); break;
        case Events: action = "events.list"; break;
        case Demons:
            m_fetch.post("demonlist", matjson::Value::object(), [this](gdps::Result r) { if (!r.ok) status(r.error); else showRows(r.value["levels"]); }, false);
            return;
        case Songs: {
            static char const* states[] = {"pending", "approved", "blocked"};
            action = "songs.list"; fields["status"] = states[std::clamp(m_sub, 0, 2)]; break;
        }
        case Roles: action = "roles.list"; break;
        case Board:
            if (m_sub == 1) { action = "scores.list"; fields["levelID"] = toInt(m_query); }
            else action = "lb.flagged";
            break;
        case Audit: action = "audit.list"; fields["page"] = m_sub; break;
        default: return;
    }
    m_fetch.admin(action, fields, [this](gdps::Result r) {
        if (!r.ok) { status(r.error); return; }
        for (auto key : {"accounts", "lists", "levels", "comments", "events", "songs", "roles", "flagged", "scores", "entries", "history", "members"}) {
            if (auto v = r.value.get(key)) { showRows(v.unwrap()); return; }
        }
        if (auto s = r.value.get("stats")) {
            auto stats = s.unwrap();
            auto arr = matjson::Value::array();
            std::pair<char const*, char const*> names[] = {
                {"accounts", "Contas"}, {"active_day", "Ativos (24 h)"}, {"active_week", "Ativos (7 dias)"}, {"banned", "Banidos"},
                {"lb_banned", "Fora do ranking"}, {"levels", "Levels"}, {"levels_day", "Levels novos (24 h)"}, {"rated", "Avaliados"},
                {"featured", "Featured"}, {"epic", "Epic ou acima"}, {"blocked", "Bloqueados"}, {"comments", "Comentarios"},
                {"downloads", "Downloads"}, {"songs", "Musicas"}, {"songs_pending", "Musicas aguardando"}, {"reports_week", "Denuncias (7 dias)"},
                {"flagged_week", "Placares suspeitos (7 dias)"}};
            for (auto [key, label] : names) arr.push(matjson::makeObject({{"label", label}, {"value", str(stats, key)}}));
            showRows(arr);
        }
    });
}

void GdpsAdminPopup::showRows(matjson::Value rows) {
    m_rows = rows;
    m_rowButtons.clear();
    std::vector<GdpsRow> out;
    ccColor3B white {255, 255, 255}, soft {190, 200, 220}, red {255, 130, 130}, gold {255, 220, 110};
    for (auto const& r : list(rows)) {
        GdpsRow row;
        switch (m_tab) {
            case Stats: row = {fmt::format("{}: {}", str(r, "label"), str(r, "value")), white, {}}; break;
            case Players:
                if (r.get("username")) {
                    row.text = fmt::format("{} (#{})  {} estrelas  {} demons  {} CP{}{}", str(r, "username"), num(r, "id"), num(r, "stars"), num(r, "demons"),
                                           num(r, "creator_points"), flag(r, "is_banned") ? "  [BANIDO]" : "", flag(r, "lb_banned") ? "  [FORA DO RANKING]" : "");
                    row.color = flag(r, "is_banned") ? red : white;
                    if (has("accounts.ban")) row.buttons.push_back(flag(r, "is_banned") ? "Desban" : "Ban");
                    if (has("leaderboard.moderate")) row.buttons.push_back(flag(r, "lb_banned") ? "LB ok" : "LB ban");
                    if (has("leaderboard.moderate")) row.buttons.push_back("Hist");
                    if (has("roles.manage")) row.buttons.push_back("Cargo");
                } else {
                    row = {fmt::format("{}  estrelas {}  demons {}  luas {}  moedas {}/{}{}", date(r, "at"), num(r, "stars"), num(r, "demons"), num(r, "moons"),
                                       num(r, "coins"), num(r, "user_coins"), flag(r, "flagged") ? "  [SUSPEITO]" : ""), flag(r, "flagged") ? red : white, {}};
                }
                break;
            case Levels:
                if (m_sub == 3) {
                    row = {fmt::format("Lista #{} {} por {}  ({} levels){}{}{}", num(r, "id"), str(r, "name"), str(r, "creator"), countIds(str(r, "level_ids")),
                                       num(r, "stars") ? fmt::format("  {} estrelas", num(r, "stars")) : std::string(), num(r, "featured") ? "  [Featured]" : "", flag(r, "deleted") ? "  [APAGADA]" : ""),
                           flag(r, "deleted") ? red : white, {}};
                    if (has("levels.rate")) row.buttons.push_back("Rate");
                    if (has("levels.delete") && !flag(r, "deleted")) row.buttons.push_back("Apagar");
                    break;
                }
                if (m_sub == 2) {
                    row = {fmt::format("{} (#{})  sugerido: {} estrelas{}  - {} voto(s) de {}", str(r, "name"), num(r, "id"), num(r, "stars"),
                                       flag(r, "feature") ? " + feature" : "", num(r, "votes"), str(r, "by")), white, {}};
                } else if (m_sub == 1) {
                    row = {fmt::format("{} (#{}) por {}  - {} denuncia(s)", str(r, "name"), num(r, "id"), str(r, "creator"), num(r, "reports")), gold, {}};
                } else {
                    row.text = fmt::format("{} (#{}) por {}  {}{}{}{}{}{}", str(r, "name"), num(r, "id"), str(r, "creator"),
                                           num(r, "stars") ? fmt::format("{} estrelas", num(r, "stars")) : std::string("sem rate"),
                                           num(r, "feature_score") > 0 ? " [Feature]" : "", tierName(num(r, "epic")),
                                           flag(r, "blocked") ? " [BLOQUEADO]" : "", flag(r, "deleted") ? " [APAGADO]" : "",
                                           num(r, "reports") ? fmt::format("  ({} denuncias)", num(r, "reports")) : std::string());
                    row.color = flag(r, "deleted") || flag(r, "blocked") ? red : white;
                }
                if (has("levels.rate")) row.buttons.push_back("Rate");
                if (has("levels.feature")) row.buttons.push_back("Tier");
                if (has("levels.block")) row.buttons.push_back(flag(r, "blocked") ? "Desbloq" : "Bloquear");
                if (has("levels.delete")) row.buttons.push_back(flag(r, "deleted") ? "Restaurar" : "Apagar");
                break;
            case Comments:
                row = {fmt::format("{}{}: {}", str(r, "kind") == "account" ? "[perfil] " : fmt::format("[level {}] ", num(r, "level_id")),
                                   str(r, "username"), decodeComment(str(r, "body"))), white, {"Apagar"}};
                break;
            case Events: {
                static char const* kinds[] = {"Daily", "Weekly", "Evento"};
                row = {fmt::format("{} #{}  {} (#{})  inicio {}{}", kinds[std::clamp<int64_t>(num(r, "kind"), 0, 2)], num(r, "id"), str(r, "name"),
                                   num(r, "level_id"), date(r, "starts_at"), flag(r, "started") ? "" : "  [agendado]"),
                       flag(r, "started") ? white : gold, {"Remover"}};
                break;
            }
            case Demons:
                row = {fmt::format("#{}  {} (#{})  por {}", num(r, "position"), str(r, "name"), num(r, "level_id"), str(r, "creator")), white, {"Remover"}};
                break;
            case Songs:
                row = {fmt::format("{}  {} - {}  por {}  ({:.1f} MB)", num(r, "id"), str(r, "name"), str(r, "artist"), str(r, "uploader"), num(r, "size") / 1048576.0), white, {}};
                if (m_sub != 1) row.buttons.push_back("Aprovar");
                if (m_sub != 2) row.buttons.push_back("Bloquear");
                row.buttons.push_back("Editar");
                row.buttons.push_back("Remover");
                break;
            case Roles:
                if (r.get("permissions")) {
                    auto perms = list(r["permissions"]);
                    row = {fmt::format("{}  (posicao {})  {} permissao(oes)  {} membro(s){}", str(r, "name"), num(r, "position"), perms.size(), num(r, "members"),
                                       flag(r, "is_owner") ? "  [dono]" : ""),
                           {static_cast<GLubyte>((num(r, "color") >> 16) & 255), static_cast<GLubyte>((num(r, "color") >> 8) & 255), static_cast<GLubyte>(num(r, "color") & 255)},
                           {"Membros"}};
                    if (has("roles.manage")) row.buttons.push_back("Editar");
                    if (has("roles.manage") && !flag(r, "is_owner")) row.buttons.push_back("Apagar");
                } else {
                    row = {fmt::format("{} (#{})", str(r, "username"), num(r, "id")), white, has("roles.manage") ? std::vector<std::string>{"Tirar"} : std::vector<std::string>{}};
                }
                break;
            case Board:
                if (m_sub == 1) {
                    row = {fmt::format("{} (#{})  {}%  moedas {}  tentativas {}  cliques {}  {} s{}", str(r, "username"), num(r, "id"), num(r, "percent"), num(r, "coins"),
                                       num(r, "attempts"), num(r, "clicks"), num(r, "seconds"), num(r, "plat_time") ? fmt::format("  tempo {} ms", num(r, "plat_time")) : std::string()),
                           white, {"Remover"}};
                } else if (m_sub == 2) {
                    row = {fmt::format("{}  estrelas {}  demons {}  luas {}  moedas {}{}", date(r, "at"), num(r, "stars"), num(r, "demons"), num(r, "moons"),
                                       num(r, "user_coins"), flag(r, "flagged") ? "  [SUSPEITO]" : ""), flag(r, "flagged") ? red : white, {}};
                } else {
                    row = {fmt::format("{} (#{})  {} estrelas  {} demons  {}{}", str(r, "username"), num(r, "id"), num(r, "stars"), num(r, "demons"), date(r, "at"),
                                       flag(r, "lb_banned") ? "  [FORA DO RANKING]" : ""), flag(r, "lb_banned") ? soft : gold,
                           {"Hist", flag(r, "lb_banned") ? "LB ok" : "LB ban", "Zerar"}};
                }
                break;
            case Audit:
                row = {fmt::format("{}  {}  {}  {} {}", date(r, "at"), str(r, "actor"), str(r, "action"), str(r, "target_type"), str(r, "target_id")), white, {}};
                break;
            case Packs:
                if (m_sub == 1) {
                    auto gid = num(r, "id");
                    row = {fmt::format("Gauntlet {} ({})  levels {}", gid, gid >= 1 && gid <= 15 ? GauntletNames[gid] : "tema do GD", str(r, "level_ids")), gold, {}};
                } else {
                    row = {fmt::format("#{} {}  levels {}  {} estrelas, {} moedas, dificuldade {}", num(r, "id"), str(r, "name"), str(r, "level_ids"), num(r, "stars"), num(r, "coins"), num(r, "difficulty")), white, {}};
                }
                if (has("packs.manage")) { row.buttons.push_back("Editar"); row.buttons.push_back("Apagar"); }
                break;
            default: break;
        }
        m_rowButtons.push_back(row.buttons);
        out.push_back(std::move(row));
    }
    auto count = out.size();
    m_list->setRows(std::move(out));
    status(count ? fmt::format("{} item(ns).", count) : "Nada encontrado.");
}

void GdpsAdminPopup::act(std::string const& action, matjson::Value fields, std::string const& done) {
    status("Enviando...");
    m_action.admin(action, std::move(fields), [this, done](gdps::Result r) {
        if (!r.ok) { status(r.error); return; }
        status(done);
        Loader::get()->queueInMainThread([self = Ref(this)] { self->load(); });
    });
}

void GdpsAdminPopup::onRow(int index, int button) {
    auto rows = list(m_rows);
    if (index < 0 || index >= int(rows.size())) return;
    auto const r = rows[index];
    if (index >= int(m_rowButtons.size())) return;
    auto const& buttons = m_rowButtons[index];
    if (button < 0 || button >= int(buttons.size())) return;
    auto const& what = buttons[button];
    auto id = num(r, "id");
    auto confirm = [this](std::string title, std::string body, std::vector<GdpsPrompt::Field> fields, std::function<void(std::vector<std::string> const&)> go) {
        if (auto p = GdpsPrompt::create(title, body, std::move(fields), {"Confirmar", "Cancelar"}, [go](int b, auto const& v) { if (b == 0) go(v); })) p->show();
    };

    if (m_tab == Players || m_tab == Board) {
        auto target = id;
        auto name = str(r, "username");
        if (what == "Ban") confirm("Banir " + name, "Dias = 0 para banimento permanente.", {{"Motivo", "", CommonFilter::Any, 120}, {"Dias", "0", CommonFilter::Uint, 4}},
            [this, target](auto const& v) { act("accounts.ban", matjson::makeObject({{"target", target}, {"reason", v[0]}, {"days", toInt(v[1])}}), "Jogador banido."); });
        else if (what == "Desban") act("accounts.unban", matjson::makeObject({{"target", target}}), "Banimento removido.");
        else if (what == "LB ban") confirm("Tirar do ranking: " + name, "So o leaderboard; o jogador continua jogando.", {{"Motivo", "", CommonFilter::Any, 120}},
            [this, target](auto const& v) { act("lb.ban", matjson::makeObject({{"target", target}, {"reason", v[0]}}), "Jogador fora do ranking."); });
        else if (what == "LB ok") act("lb.unban", matjson::makeObject({{"target", target}}), "Jogador de volta ao ranking.");
        else if (what == "Zerar") confirm("Zerar estatisticas de " + name, "Estrelas, luas, demons e moedas vao a 0 (o historico fica).", {},
            [this, target](auto const&) { act("lb.reset", matjson::makeObject({{"target", target}}), "Estatisticas zeradas."); });
        else if (what == "Hist") {
            status("Carregando historico...");
            m_fetch.admin("lb.history", matjson::makeObject({{"target", target}}), [this](gdps::Result h) {
                if (!h.ok) { status(h.error); return; }
                if (m_tab == Board) m_sub = 2;
                showRows(h.value["history"]);
            });
        } else if (what == "Cargo") {
            std::string names;
            for (auto const& role : list(m_roles)) names += fmt::format("{}{}={}", names.empty() ? "" : ", ", num(role, "role_id"), str(role, "name"));
            auto prompt = GdpsPrompt::create("Cargo de " + name, names, {{"ID do cargo", "", CommonFilter::Uint, 6}}, {"Dar", "Tirar", "Cancelar"},
                [this, target](int b, auto const& v) {
                    if (b == 2) return;
                    act(b == 0 ? "roles.grant" : "roles.revoke", matjson::makeObject({{"target", target}, {"role_id", toInt(v[0])}}), b == 0 ? "Cargo dado." : "Cargo retirado.");
                });
            if (prompt) prompt->show();
        } else if (what == "Remover" && m_tab == Board) {
            auto level = toInt(m_query);
            confirm("Remover pontuacao de " + name, "", {}, [this, level, target](auto const&) {
                act("scores.remove", matjson::makeObject({{"levelID", level}, {"target", target}}), "Pontuacao removida.");
            });
        }
        return;
    }
    if (m_tab == Levels && m_sub == 3) {
        if (what == "Rate") confirm("Lista: " + str(r, "name"), "Estrelas da lista, Featured (0/1) e quantos levels completar para ganhar.",
            {{"Estrelas", str(r, "stars"), CommonFilter::Uint, 3}, {"Featured (0/1)", num(r, "featured") ? "1" : "0", CommonFilter::Uint, 1}, {"Levels para o premio", "0", CommonFilter::Uint, 3}},
            [this, id](auto const& v) { act("lists.rate", matjson::makeObject({{"listID", id}, {"stars", toInt(v[0])}, {"featured", toInt(v[1])}, {"count_for_reward", toInt(v[2])}}), "Lista avaliada."); });
        else if (what == "Apagar") confirm("Apagar lista " + str(r, "name"), "", {}, [this, id](auto const&) { act("lists.delete", matjson::makeObject({{"listID", id}}), "Lista apagada."); });
        return;
    }
    if (m_tab == Packs) {
        if (m_sub == 1) {
            if (what == "Editar") editGauntlet(r);
            else if (what == "Apagar") confirm(fmt::format("Apagar Gauntlet {}", id), "", {}, [this, id](auto const&) { act("gauntlets.delete", matjson::makeObject({{"gauntlet_id", id}}), "Gauntlet apagada."); });
        } else {
            if (what == "Editar") editPack(r);
            else if (what == "Apagar") confirm("Apagar " + str(r, "name"), "", {}, [this, id](auto const&) { act("packs.delete", matjson::makeObject({{"pack_id", id}}), "Map Pack apagado."); });
        }
        return;
    }
    if (m_tab == Levels) {
        auto name = str(r, "name");
        if (what == "Rate") {
            // The same RobTop-style popup as the level page (face = Featured/Epic/...).
            int tier = jsonutil::integerOr(r, "epic", 0) > 0 ? static_cast<int>(jsonutil::integerOr(r, "epic", 0)) + 1 : (jsonutil::integerOr(r, "feature_score", 0) > 0 ? 1 : 0);
            if (auto p = GdpsRatePopup::create(id, name, static_cast<int>(jsonutil::integerOr(r, "stars", 0)), tier, [weak = WeakRef<GdpsAdminPopup>(this)] { if (auto self = weak.lock()) self->load(); })) p->show();
        } else if (what == "Tier") {
            auto prompt = GdpsPrompt::create("Destaque: " + name, "Precisa ter rate (estrelas).", {}, {"Nada", "Feature", "Epic", "Legendary", "Mythic"},
                [this, id](int b, auto const&) {
                    act("levels.feature", matjson::makeObject({{"levelID", id}, {"feature", b >= 1}, {"epic", std::max(0, b - 1)}}), "Destaque salvo.");
                });
            if (prompt) prompt->show();
        } else if (what == "Bloquear") confirm("Bloquear " + name, "Some das buscas e downloads.", {{"Motivo", "", CommonFilter::Any, 120}},
            [this, id](auto const& v) { act("levels.block", matjson::makeObject({{"levelID", id}, {"reason", v[0]}}), "Level bloqueado."); });
        else if (what == "Desbloq") act("levels.unblock", matjson::makeObject({{"levelID", id}}), "Level desbloqueado.");
        else if (what == "Apagar") confirm("Apagar " + name, "Pode ser restaurado depois.", {{"Motivo", "", CommonFilter::Any, 120}},
            [this, id](auto const& v) { act("levels.delete", matjson::makeObject({{"levelID", id}, {"reason", v[0]}}), "Level apagado."); });
        else if (what == "Restaurar") act("levels.restore", matjson::makeObject({{"levelID", id}}), "Level restaurado.");
        return;
    }
    if (m_tab == Comments) {
        auto kind = str(r, "kind");
        confirm("Apagar comentario", "", {}, [this, id, kind](auto const&) {
            act("comments.delete", matjson::makeObject({{"commentID", id}, {"kind", kind}}), "Comentario apagado.");
        });
        return;
    }
    if (m_tab == Events) { confirm("Remover evento", "", {}, [this, id](auto const&) { act("events.remove", matjson::makeObject({{"id", id}}), "Evento removido."); }); return; }
    if (m_tab == Demons) {
        auto level = num(r, "level_id");
        confirm("Tirar da Demon List", "", {}, [this, level](auto const&) { act("demonlist.remove", matjson::makeObject({{"levelID", level}}), "Removido da lista."); });
        return;
    }
    if (m_tab == Songs) {
        if (what == "Editar") {
            auto prompt = GdpsPrompt::create("Editar musica", "", {{"Nome", str(r, "name"), CommonFilter::Any, 60}, {"Artista", str(r, "artist"), CommonFilter::Any, 40}},
                {"Salvar", "Cancelar"}, [this, id](int b, auto const& v) {
                    if (b == 0) act("songs.edit", matjson::makeObject({{"songID", id}, {"name", v[0]}, {"artist", v[1]}}), "Musica editada.");
                });
            if (prompt) prompt->show();
            return;
        }
        std::string state = what == "Aprovar" ? "approved" : what == "Bloquear" ? "blocked" : "removed";
        auto go = [this, id, state, what](auto const&) { act("songs.review", matjson::makeObject({{"songID", id}, {"status", state}}), what == "Aprovar" ? "Musica aprovada." : "Musica atualizada."); };
        if (state == "removed") confirm("Remover musica", "O arquivo e apagado; o ID nunca e reutilizado.", {}, go);
        else go(std::vector<std::string>{});
        return;
    }
    if (m_tab == Roles) {
        if (r.get("permissions")) {
            auto roleId = num(r, "role_id");
            if (what == "Membros") {
                status("Carregando membros...");
                m_sub = 1;
                m_fetch.admin("roles.members", matjson::makeObject({{"role_id", roleId}}), [this, roleId](gdps::Result m) {
                    if (!m.ok) { status(m.error); return; }
                    auto members = m.value["members"];
                    if (members.isArray()) for (auto& x : members.asArray().unwrap()) x["role_id"] = roleId;
                    showRows(members);
                });
            } else if (what == "Editar") {
                m_fetch.admin("roles.list", matjson::Value::object(), [this, r](gdps::Result l) {
                    if (!l.ok) { status(l.error); return; }
                    if (auto p = GdpsRolePopup::create(r, l.value["catalog"], [this] { load(); })) p->show();
                });
            } else if (what == "Apagar") {
                confirm("Apagar cargo " + str(r, "name"), "Quem tem o cargo perde as permissoes dele.", {},
                    [this, roleId](auto const&) { act("roles.delete", matjson::makeObject({{"role_id", roleId}}), "Cargo apagado."); });
            }
        } else if (what == "Tirar") {
            auto roleId = num(r, "role_id");
            confirm("Tirar cargo de " + str(r, "username"), "", {}, [this, id, roleId](auto const&) {
                act("roles.revoke", matjson::makeObject({{"target", id}, {"role_id", roleId}}), "Cargo retirado.");
            });
        }
    }
}

namespace {
std::vector<std::string> splitIds(std::string const& text) {
    std::vector<std::string> out;
    std::string current;
    for (char c : text + ",") {
        if (std::isdigit(static_cast<unsigned char>(c))) current += c;
        else if (!current.empty()) { out.push_back(current); current.clear(); }
    }
    return out;
}
std::string joinIds(std::vector<std::string> const& ids) {
    std::string out;
    for (auto const& id : ids) if (!id.empty()) out += (out.empty() ? "" : ",") + id;
    return out;
}
} // namespace

// The GeoBR panel (news, notices, pass, quests, Main Levels, shop, ranking,
// accounts) for the GDPS Owner: the Owner's GDPS account opens the 20-minute
// admin session on the server, instead of the old panel password.
void GdpsAdminPopup::openGeoBRPanel() {
    auto am = GJAccountManager::sharedState();
    if (!am || gdps::accountID() <= 0) { status("Entre na conta GDPS do Owner."); return; }
    status("Abrindo o Painel GeoBR...");
    WeakRef<GdpsAdminPopup> weak = this;
    m_bridge.rpc("geobr_admin_login_gdps", matjson::makeObject({{"p_account", gdps::accountID()}, {"p_gjp2", std::string(am->m_GJP2)}}), [weak](auto r) {
        auto self = weak.lock();
        if (!self) return;
        if (!r.success || !jsonutil::boolOr(r.value, "ok", false)) {
            self->status(r.success ? jsonutil::stringOr(r.value, "error", "Acesso negado.") : r.error);
            return;
        }
        self->status("Painel GeoBR aberto (sessao de 20 minutos).");
        if (auto panel = AdminPanel::create()) panel->show();
    });
}

void GdpsAdminPopup::editPack(matjson::Value pack) {
    auto id = num(pack, "id");
    // One field per level ID (GD's Map Packs have 3 levels).
    auto ids = splitIds(str(pack, "level_ids"));
    auto idAt = [&](size_t i) { return i < ids.size() ? ids[i] : std::string(); };
    auto prompt = GdpsPrompt::create(id ? fmt::format("Map Pack #{}", id) : "Novo Map Pack",
        "Dificuldade: 0 Auto a 5 Insane, 6 a 10 Demons. Cor: #RRGGBB.",
        {{"Nome", str(pack, "name"), CommonFilter::Any, 40},
         {"ID level 1", idAt(0), CommonFilter::Uint, 10}, {"ID level 2", idAt(1), CommonFilter::Uint, 10}, {"ID level 3", idAt(2), CommonFilter::Uint, 10},
         {"Estrelas", id ? str(pack, "stars") : "0", CommonFilter::Uint, 2}, {"Moedas", id ? str(pack, "coins") : "0", CommonFilter::Uint, 2},
         {"Dificuldade", id ? str(pack, "difficulty") : "0", CommonFilter::Uint, 2}, {"Cor", str(pack, "text_color").empty() ? "#ffffff" : str(pack, "text_color"), CommonFilter::Any, 11}},
        {"Salvar", "Cancelar"}, [this, id](int b, auto const& v) {
            if (b != 0) return;
            auto body = matjson::makeObject({{"name", v[0]}, {"levels", joinIds({v[1], v[2], v[3]})}, {"stars", toInt(v[4])}, {"coins", toInt(v[5])}, {"difficulty", toInt(v[6])}, {"text_color", v[7]}, {"bar_color", v[7]}});
            if (id) body["pack_id"] = id;
            act("packs.save", body, "Map Pack salvo.");
        });
    if (prompt) prompt->show();
}

void GdpsAdminPopup::editGauntlet(matjson::Value gauntlet) {
    auto id = num(gauntlet, "id");
    auto prompt = GdpsPrompt::create(id ? fmt::format("Gauntlet {}", id) : "Gauntlet",
        "Tipo: 1 Fire, 2 Ice, 3 Poison, 4 Shadow, 5 Lava, 6 Bonus, 7 Chaos, 8 Demon, 9 Time, 10 Crystal... Exatamente 5 levels.",
        [&] {
            auto ids = splitIds(str(gauntlet, "level_ids"));
            std::vector<GdpsPrompt::Field> fields = {{"Tipo (1-99)", id ? std::to_string(id) : "1", CommonFilter::Uint, 2}};
            for (size_t i = 0; i < 5; ++i) fields.push_back({fmt::format("ID level {}", i + 1), i < ids.size() ? ids[i] : std::string(), CommonFilter::Uint, 10});
            return fields;
        }(),
        {"Salvar", "Cancelar"}, [this](int b, auto const& v) {
            if (b != 0) return;
            act("gauntlets.save", matjson::makeObject({{"gauntlet_id", toInt(v[0])}, {"levels", joinIds({v[1], v[2], v[3], v[4], v[5]})}}), "Gauntlet salva.");
        });
    if (prompt) prompt->show();
}

// ------------------------------------------------------------- role editor
GdpsRolePopup* GdpsRolePopup::create(matjson::Value role, matjson::Value catalog, Saved saved) {
    auto p = new GdpsRolePopup;
    if (p->init(std::move(role), std::move(catalog), std::move(saved))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool GdpsRolePopup::init(matjson::Value role, matjson::Value catalog, Saved saved) {
    CCSize size {470, 300};
    if (!Popup::init(size.width, size.height)) return false;
    visual::fitPopup(m_mainLayer, size);
    visual::stylePopup(m_mainLayer, m_bgSprite, size, {60, 44, 90});
    m_role = std::move(role);
    m_saved = std::move(saved);
    bool editing = m_role.get("role_id").isOk();
    setTitle(editing ? "Editar cargo" : "Novo cargo", "goldFont.fnt", .6f, 14);
    for (auto const& c : list(catalog)) m_catalog.push_back(c.asString().unwrapOr(""));
    if (m_catalog.empty()) m_catalog = {"admin.panel", "accounts.view", "accounts.ban", "accounts.manage", "roles.manage", "levels.rate", "levels.feature",
                                        "levels.suggest", "levels.delete", "levels.block", "comments.moderate", "events.manage", "leaderboard.moderate",
                                        "songs.moderate", "stats.view", "audit.view", "mod.badge", "mod.elder", "packs.manage"};
    auto field = [&](char const* label, std::string value, float x, float w, CommonFilter filter, int max) {
        auto l = ui::makeLabel(label, "chatFont.fnt", .42f, {216, 236, 255});
        l->setPosition({x, 262});
        m_mainLayer->addChild(l, 4);
        auto input = TextInput::create(w, label);
        input->setCommonFilter(filter);
        input->setMaxCharCount(max);
        input->setString(value);
        input->setPosition({x, 240});
        m_mainLayer->addChild(input, 4);
        return input;
    };
    auto color = jsonutil::integerOr(m_role, "color", 0xffffff);
    m_name = field("Nome", str(m_role, "name"), 110, 170, CommonFilter::Any, 32);
    m_color = field("Cor (hex)", fmt::format("{:06X}", color), 268, 100, CommonFilter::Hex, 6);
    m_position = field("Posicao", editing ? str(m_role, "position") : "100", 390, 80, CommonFilter::Uint, 4);
    std::set<std::string> held;
    if (auto perms = m_role.get("permissions")) for (auto const& p : list(perms.unwrap())) held.insert(p.asString().unwrapOr(""));
    bool owner = jsonutil::boolOr(m_role, "is_owner", false);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 5);
    for (size_t i = 0; i < m_catalog.size(); ++i) {
        float x = 30.f + (i % 3) * 148.f, y = 206.f - (i / 3) * 24.f;
        auto toggle = CCMenuItemToggler::createWithStandardSprites(this, nullptr, .45f);
        toggle->toggle(owner || held.contains(m_catalog[i]) || held.contains("*"));
        toggle->setEnabled(!owner);
        toggle->setPosition({x, y});
        menu->addChild(toggle);
        m_toggles.push_back(toggle);
        auto l = ui::makeLabel(m_catalog[i], "chatFont.fnt", .4f, {230, 240, 255});
        l->setAnchorPoint({0, .5f});
        l->setPosition({x + 14, y});
        l->limitLabelWidth(126, .4f, .2f);
        m_mainLayer->addChild(l, 4);
    }
    if (owner) {
        m_position->setEnabled(false);
        auto note = ui::makeLabel("O cargo Owner sempre tem todas as permissoes.", "chatFont.fnt", .4f, ui::palette::TextGold);
        note->setPosition({size.width / 2, 52});
        m_mainLayer->addChild(note, 4);
    }
    auto save = ui::makeTextButton("Salvar", 120, "GJ_button_01.png", this, menu_selector(GdpsRolePopup::onSave), .36f, 28);
    save->setPosition({size.width / 2, 24});
    menu->addChild(save);
    m_status = ui::makeLabel("", "chatFont.fnt", .4f, ui::palette::TextSoft);
    m_status->setPosition({size.width / 2, 52});
    m_mainLayer->addChild(m_status, 4);
    return true;
}

void GdpsRolePopup::onSave(CCObject*) {
    auto perms = matjson::Value::array();
    for (size_t i = 0; i < m_toggles.size(); ++i) if (m_toggles[i]->isToggled()) perms.push(m_catalog[i]);
    int color = 0xffffff;
    try { color = std::stoi(std::string(m_color->getString()), nullptr, 16); } catch (...) {}
    auto body = matjson::makeObject({{"name", std::string(m_name->getString())}, {"color", color}, {"position", toInt(std::string(m_position->getString()), 100)}, {"permissions", perms}});
    if (auto id = m_role.get("role_id")) body["role_id"] = id.unwrap();
    m_status->setString("Salvando...");
    m_save.admin("roles.save", body, [this](gdps::Result r) {
        if (!r.ok) { m_status->setString(r.error.c_str()); m_status->limitLabelWidth(440, .4f, .2f); return; }
        auto saved = m_saved;
        onClose(nullptr);
        if (saved) saved();
    });
}

} // namespace geobr
