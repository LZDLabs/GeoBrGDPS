#include "CollabRoomPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "SocialPopups.hpp"
#include "UIHelpers.hpp"
#include "../collab/CollabLive.hpp"
#include "../collab/CollabService.hpp"
#include "../models/JsonModelUtils.hpp"

#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/general.hpp>

#include <algorithm>

using namespace geode::prelude;

namespace geobr {
namespace {

constexpr float W = 440.f, H = 290.f;

void label(CCNode* parent, std::string const& text, CCPoint pos, float scale, float width, ccColor3B color = {255, 255, 255},
    char const* font = "bigFont.fnt", float anchorX = .5f) {
    auto l = ui::makeLabel(text, font, scale, color);
    l->setAnchorPoint({anchorX, .5f});
    l->setPosition(pos);
    l->limitLabelWidth(width, scale, .15f);
    parent->addChild(l);
}

CCMenuItemSpriteExtra* button(CCMenu* menu, std::string const& text, CCPoint pos, float width, char const* bg, CCObject* target,
    SEL_MenuHandler handler, int tag = 0, bool enabled = true) {
    auto b = ui::makeTextButton(text, static_cast<int>(width), bg, target, handler, .26f, 24);
    b->setPosition(pos);
    b->setTag(tag);
    b->setEnabled(enabled);
    menu->addChild(b);
    return b;
}

std::string hhmm(std::string const& stamp) { return stamp.size() > 15 ? stamp.substr(11, 5) : std::string(); }

std::string actionText(matjson::Value const& l) {
    auto action = jsonutil::stringOr(l, "action");
    auto actor = jsonutil::stringOr(l, "actor", "Alguem");
    auto target = jsonutil::stringOr(l, "target");
    auto detail = jsonutil::stringOr(l, "detail");
    if (action == "kick") return actor + " expulsou " + target;
    if (action == "ban") return actor + " baniu " + target;
    if (action == "unban") return actor + " desbaniu " + target;
    if (action == "mute") return actor + " silenciou " + target;
    if (action == "unmute") return actor + " liberou o chat de " + target;
    if (action == "viewer") return actor + " bloqueou a edicao de " + target;
    if (action == "editor") return actor + " liberou a edicao de " + target;
    if (action == "revert") return target.empty() ? actor + " desfez a sala (" + detail + ")" : actor + " reverteu " + target + " (" + detail + ")";
    if (action == "clean_yellow") return actor + " apagou portais amarelos (" + detail + ")";
    if (action == "clean_dupes") return actor + " apagou duplicados (" + detail + ")";
    if (action == "restore") return actor + " reiniciou a sala (" + detail + ")";
    if (action == "resync") return actor + " recarregou a sala para todos";
    if (action == "code") return actor + " trocou o codigo";
    if (action == "lock") return actor + " trancou a entrada";
    if (action == "unlock") return actor + " abriu a entrada";
    if (action == "leave") return actor + " saiu da sala";
    return actor + ": " + action;
}

} // namespace

// ------------------------------------------------------------- room panel
CollabRoomPopup* CollabRoomPopup::create(std::string room, EditorUI* editor) {
    auto p = new CollabRoomPopup;
    if (p->init(std::move(room), editor)) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool CollabRoomPopup::init(std::string room, EditorUI* editor) {
    if (!Popup::init(W, H)) return false;
    visual::fitPopup(m_mainLayer, {W, H});
    visual::stylePopup(m_mainLayer, m_bgSprite, {W, H}, {30, 44, 82});
    setTitle("Sala", "goldFont.fnt", .65f, 16);
    setID("geobr-collab-room");
    m_room = std::move(room);
    m_editor = WeakRef<EditorUI>(editor);
    m_status = ui::makeLabel("Carregando...", "chatFont.fnt", .38f, {174, 209, 237});
    m_status->setPosition({W / 2, 13});
    m_mainLayer->addChild(m_status, 5);
    fetch();
    return true;
}

int CollabRoomPopup::rank() const { return static_cast<int>(jsonutil::integerOr(m_info, "rank", 1)); }

void CollabRoomPopup::setStatus(std::string const& text) {
    if (!m_status) return;
    m_status->setString(text.c_str());
    m_status->limitLabelWidth(W - 30, .38f, .2f);
}

void CollabRoomPopup::fetch() {
    WeakRef<CollabRoomPopup> weak = this;
    m_fetch.rpc("geobr_collab_room_info", matjson::makeObject({{"p_room", m_room}}), [weak](auto r) {
        auto self = weak.lock();
        if (!self) return;
        if (!r.success) { self->setStatus(r.error); return; }
        self->m_info = r.value;
        self->render();
    });
}

void CollabRoomPopup::render() {
    if (m_content) m_content->removeFromParent();
    m_content = CCNode::create();
    m_mainLayer->addChild(m_content, 3);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_content->addChild(menu, 2);
    if (m_view == View::Bans) renderBans(menu);
    else if (m_view == View::Log) { renderLog(); button(menu, "Voltar", {W / 2, 36}, 90, "GJ_button_04.png", this, menu_selector(CollabRoomPopup::onBack)); }
    else renderMain(menu);
}

void CollabRoomPopup::renderMain(CCMenu* menu) {
    auto const& i = m_info;
    int r = rank();
    std::string title = jsonutil::stringOr(i, "title", "Sala");
    if (m_title) { m_title->setString(("Sala: " + title).c_str()); m_title->limitLabelWidth(W - 80, .65f, .3f); }
    bool locked = jsonutil::boolOr(i, "locked", false);
    label(m_content, fmt::format("Codigo: {}   |   {} membros   |   {} objetos   |   entrada {}", jsonutil::stringOr(i, "code"),
        jsonutil::integerOr(i, "members"), jsonutil::integerOr(i, "objects"), locked ? "trancada" : "aberta"),
        {W / 2, H - 44}, .3f, W - 40, {200, 225, 250}, "chatFont.fnt");
    button(menu, "Copiar codigo", {W - 62, H - 66}, 90, "GJ_button_04.png", this, menu_selector(CollabRoomPopup::onCopyCode));

    auto editor = m_editor.lock();
    size_t loose = editor ? collab::countLooseYellowPortals(editor->m_editorLayer) : 0;
    auto yellow = jsonutil::integerOr(i, "yellow");
    auto dupes = jsonutil::integerOr(i, "dupes");
    bool mod = r >= 2, host = r >= 3;

    float y = H - 88;
    label(m_content, "Limpar", {24, y}, .34f, 120, ui::palette::TextGold, "bigFont.fnt", 0.f);
    button(menu, fmt::format("Portais amarelos ({})", yellow + static_cast<int64_t>(loose)), {150, y}, 150, "GJ_button_06.png", this,
        menu_selector(CollabRoomPopup::onYellow), 0, yellow + static_cast<int64_t>(loose) > 0);
    button(menu, fmt::format("Duplicados ({})", dupes), {310, y}, 130, "GJ_button_06.png", this, menu_selector(CollabRoomPopup::onDupes), 0, mod && dupes > 0);

    y -= 34;
    label(m_content, "Desfazer", {24, y}, .34f, 120, ui::palette::TextGold, "bigFont.fnt", 0.f);
    int minutes[] = {5, 15, 60};
    char const* names[] = {"Ultimos 5 min", "Ultimos 15 min", "Ultima 1 hora"};
    for (int k = 0; k < 3; ++k)
        button(menu, names[k], {150.f + k * 105.f, y}, 98, "GJ_button_05.png", this, menu_selector(CollabRoomPopup::onUndoRoom), minutes[k], mod);

    y -= 34;
    label(m_content, "Sala", {24, y}, .34f, 120, ui::palette::TextGold, "bigFont.fnt", 0.f);
    button(menu, "Recarregar todos", {150, y}, 98, "GJ_button_02.png", this, menu_selector(CollabRoomPopup::onReloadAll), 0, mod);
    button(menu, locked ? "Abrir entrada" : "Trancar entrada", {255, y}, 98, "GJ_button_02.png", this, menu_selector(CollabRoomPopup::onLock), 0, mod);
    button(menu, "Novo codigo", {360, y}, 98, "GJ_button_02.png", this, menu_selector(CollabRoomPopup::onNewCode), 0, host);

    y -= 34;
    label(m_content, "Reiniciar", {24, y}, .34f, 120, ui::palette::TextGold, "bigFont.fnt", 0.f);
    button(menu, "Voltar ao ultimo save", {165, y}, 128, "GJ_button_06.png", this, menu_selector(CollabRoomPopup::onRestoreSave), 0, host);
    button(menu, "Apagar tudo", {300, y}, 110, "GJ_button_06.png", this, menu_selector(CollabRoomPopup::onClearAll), 0, host);

    y -= 38;
    auto bans = i["bans"].isArray() ? i["bans"].asArray().unwrap().size() : 0;
    button(menu, fmt::format("Banidos ({})", bans), {105, y}, 110, "GJ_button_04.png", this, menu_selector(CollabRoomPopup::onShowBans), 0, mod);
    button(menu, "Registro", {220, y}, 90, "GJ_button_04.png", this, menu_selector(CollabRoomPopup::onShowLog), 0, mod);
    button(menu, "Recarregar so eu", {335, y}, 110, "GJ_button_04.png", this, menu_selector(CollabRoomPopup::onReloadMe), 0, static_cast<bool>(editor));

    if (!mod) setStatus("Somente o host e os ADMs moderam a sala. Voce pode apagar portais soltos e recarregar.");
    else {
        auto from = jsonutil::stringOr(i, "history_from");
        setStatus(from.empty() ? "Desfazer usa o historico da sala (comeca a contar agora)." : "Historico desde " + hhmm(from) + " (as ultimas 30 mil mudancas).");
    }
}

void CollabRoomPopup::renderBans(CCMenu* menu) {
    auto rows = m_info["bans"].isArray() ? m_info["bans"].asArray().unwrap() : std::vector<matjson::Value>{};
    int pages = std::max<int>(1, (static_cast<int>(rows.size()) + 4) / 5);
    m_banPage = std::clamp(m_banPage, 0, pages - 1);
    label(m_content, "Banidos desta sala", {W / 2, H - 46}, .4f, 300, ui::palette::TextGold);
    if (rows.empty()) label(m_content, "Ninguem banido.", {W / 2, H / 2}, .4f, 300, {190, 214, 240}, "chatFont.fnt");
    for (int k = 0; k < 5; ++k) {
        size_t index = m_banPage * 5 + k;
        if (index >= rows.size()) break;
        float y = H - 80 - k * 34;
        auto panel = ui::makePanel({340, 28}, {22, 32, 62}, 245);
        panel->setPosition({190, y});
        m_content->addChild(panel);
        label(m_content, jsonutil::stringOr(rows[index], "display_name", "Jogador"), {30, y}, .32f, 220, {255, 255, 255}, "bigFont.fnt", 0.f);
        label(m_content, hhmm(jsonutil::stringOr(rows[index], "created_at")), {320, y}, .3f, 60, {170, 205, 240}, "chatFont.fnt");
        button(menu, "Desbanir", {395, y}, 70, "GJ_button_01.png", this, menu_selector(CollabRoomPopup::onUnban), static_cast<int>(index));
    }
    if (pages > 1) {
        button(menu, "<", {40, 36}, 36, "GJ_button_04.png", this, menu_selector(CollabRoomPopup::onBanPage), -1, m_banPage > 0);
        button(menu, ">", {W - 40, 36}, 36, "GJ_button_04.png", this, menu_selector(CollabRoomPopup::onBanPage), 1, m_banPage + 1 < pages);
    }
    button(menu, "Voltar", {W / 2, 36}, 90, "GJ_button_04.png", this, menu_selector(CollabRoomPopup::onBack));
    setStatus("Quem foi banido nao entra mais com o codigo.");
}

void CollabRoomPopup::renderLog() {
    label(m_content, "Registro da sala", {W / 2, H - 46}, .4f, 300, ui::palette::TextGold);
    auto panel = ui::makePanel({W - 40, 170}, {14, 24, 43}, 255);
    panel->setPosition({W / 2, H / 2 - 4});
    m_content->addChild(panel);
    auto scroll = ScrollLayer::create({W - 52, 162}, true, true);
    scroll->setPosition({26, H / 2 - 85});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(2));
    m_content->addChild(scroll, 2);
    auto rows = m_info["log"].isArray() ? m_info["log"].asArray().unwrap() : std::vector<matjson::Value>{};
    if (rows.empty()) {
        auto hint = ui::makeLabel("Nada ainda.", "chatFont.fnt", .45f, {190, 214, 240});
        hint->setPosition({(W - 52) / 2, 81});
        scroll->m_contentLayer->addChild(hint);
    }
    for (auto const& l : rows) {
        auto line = CCNode::create();
        line->setContentSize({W - 60, 18});
        line->setAnchorPoint({.5f, .5f});
        label(line, hhmm(jsonutil::stringOr(l, "at")), {4, 9}, .42f, 40, {150, 185, 215}, "chatFont.fnt", 0.f);
        label(line, actionText(l), {46, 9}, .42f, W - 120, {235, 240, 250}, "chatFont.fnt", 0.f);
        scroll->m_contentLayer->addChild(line);
    }
    scroll->m_contentLayer->updateLayout();
    scroll->scrollToTop();
    setStatus("As ultimas 20 acoes de moderacao.");
}

void CollabRoomPopup::run(std::string fn, matjson::Value args, std::string done, bool refetch) {
    if (m_busy) return;
    m_busy = true;
    setStatus("Aguarde...");
    WeakRef<CollabRoomPopup> weak = this;
    m_action.rpc(fn, std::move(args), [weak, done, refetch](auto r) {
        auto self = weak.lock();
        if (!self) return;
        self->m_busy = false;
        if (!r.success) { self->setStatus(r.error); return; }
        std::string text = done;
        if (r.value.contains("reverted")) text = fmt::format("{}: {} objetos", done, jsonutil::integerOr(r.value, "reverted"));
        if (r.value.contains("removed") && !r.value.contains("objects")) text = fmt::format("{}: {} objetos", done, jsonutil::integerOr(r.value, "removed"));
        if (r.value.contains("code") && r.value["code"].isString()) text = done + ": " + jsonutil::stringOr(r.value, "code");
        Notification::create(text, NotificationIcon::Success, 2.f)->show();
        self->setStatus(text);
        if (refetch) self->fetch();
    });
}

void CollabRoomPopup::confirm(std::string title, std::string text, std::string yes, std::function<void()> action) {
    createQuickPopup(title.c_str(), text, "Cancelar", yes.c_str(), [action](auto, bool ok) { if (ok) action(); });
}

void CollabRoomPopup::onYellow(CCObject*) {
    WeakRef<CollabRoomPopup> weak = this;
    confirm("Portais amarelos", "Apagar os <cy>portais amarelos soltos</c> (as copias do bug, sem portal azul)?\nOs portais de teleporte de verdade ficam.", "Apagar",
        [weak] {
            auto self = weak.lock();
            if (!self) return;
            size_t here = 0;
            if (auto editor = self->m_editor.lock()) here = collab::deleteLooseYellowPortals(editor->m_editorLayer);
            if (self->rank() >= 2) self->run("geobr_collab_clean", matjson::makeObject({{"p_room", self->m_room}, {"p_what", "yellow"}}), "Portais amarelos apagados da sala");
            else { Notification::create(fmt::format("{} portais amarelos apagados", here), NotificationIcon::Success, 2.f)->show(); self->fetch(); }
        });
}

void CollabRoomPopup::onDupes(CCObject*) {
    WeakRef<CollabRoomPopup> weak = this;
    confirm("Duplicados", "Apagar os objetos <cy>repetidos</c> (copias exatas no mesmo lugar)? Fica um de cada.\nDa para desfazer em Desfazer.", "Apagar", [weak] {
        if (auto self = weak.lock()) self->run("geobr_collab_clean", matjson::makeObject({{"p_room", self->m_room}, {"p_what", "dupes"}}), "Duplicados apagados");
    });
}

void CollabRoomPopup::onUndoRoom(CCObject* sender) {
    int minutes = sender->getTag();
    WeakRef<CollabRoomPopup> weak = this;
    confirm("Desfazer a sala", fmt::format("Voltar a sala para como estava ha <cy>{}</c>? Tudo que todo mundo fez nesse tempo sai.",
        minutes >= 60 ? std::string("1 hora") : fmt::format("{} minutos", minutes)), "Desfazer", [weak, minutes] {
        if (auto self = weak.lock())
            self->run("geobr_collab_revert", matjson::makeObject({{"p_room", self->m_room}, {"p_user", nullptr}, {"p_minutes", minutes}}), "Sala desfeita");
    });
}

void CollabRoomPopup::onReloadAll(CCObject*) {
    run("geobr_collab_resync", matjson::makeObject({{"p_room", m_room}}), "Todos vao recarregar a sala");
}

void CollabRoomPopup::onReloadMe(CCObject*) {
    if (auto editor = m_editor.lock()) collab::reloadRoom(editor);
    Notification::create("Recarregando a sala...", NotificationIcon::Info, 1.5f)->show();
    onClose(nullptr);
}

void CollabRoomPopup::onLock(CCObject*) {
    bool locked = jsonutil::boolOr(m_info, "locked", false);
    run("geobr_collab_lock", matjson::makeObject({{"p_room", m_room}, {"p_on", !locked}}), locked ? "Entrada aberta" : "Entrada trancada");
}

void CollabRoomPopup::onNewCode(CCObject*) {
    WeakRef<CollabRoomPopup> weak = this;
    confirm("Novo codigo", "Trocar o codigo da sala? O codigo antigo para de funcionar (quem ja esta na sala continua).", "Trocar", [weak] {
        if (auto self = weak.lock()) self->run("geobr_collab_new_code", matjson::makeObject({{"p_room", self->m_room}}), "Novo codigo");
    });
}

void CollabRoomPopup::onCopyCode(CCObject*) {
    auto code = jsonutil::stringOr(m_info, "code");
    if (code.empty()) return;
    utils::clipboard::write(code);
    Notification::create("Codigo copiado: " + code, NotificationIcon::Success, 1.5f)->show();
}

void CollabRoomPopup::onRestoreSave(CCObject*) {
    WeakRef<CollabRoomPopup> weak = this;
    confirm("Voltar ao ultimo save", "A sala volta a ser o <cy>ultimo level salvo</c> (Salvar no menu de pausa). Todo mundo recarrega.", "Voltar", [weak] {
        auto self = weak.lock();
        if (!self || self->m_busy) return;
        self->m_busy = true;
        self->setStatus("Baixando o ultimo save...");
        self->m_pull.rpc("geobr_collab_pull", matjson::makeObject({{"p_room", self->m_room}}), [weak](auto r) {
            auto self = weak.lock();
            if (!self) return;
            self->m_busy = false;
            if (!r.success) { self->setStatus(r.error); return; }
            auto level = collab::CollabService::decompress(jsonutil::stringOr(r.value, "level"));
            auto objects = matjson::Value::array();
            size_t start = 0;
            bool header = true;
            while (start <= level.size()) {
                auto end = level.find(';', start);
                auto part = level.substr(start, end == std::string::npos ? std::string::npos : end - start);
                if (!header && part.starts_with("1,")) objects.push(part);
                header = false;
                if (end == std::string::npos) break;
                start = end + 1;
            }
            if (objects.asArray().unwrap().empty()) { self->setStatus("A sala ainda nao tem um save com objetos."); return; }
            self->run("geobr_collab_restore", matjson::makeObject({{"p_room", self->m_room}, {"p_objects", objects}, {"p_label", "ultimo save"}}),
                "Sala voltou ao ultimo save");
        });
    });
}

void CollabRoomPopup::onClearAll(CCObject*) {
    WeakRef<CollabRoomPopup> weak = this;
    confirm("Apagar tudo", "Apagar <cr>todos os objetos</c> da sala para todo mundo? So o host pode fazer isso.", "Apagar tudo", [weak] {
        if (auto self = weak.lock())
            self->run("geobr_collab_restore", matjson::makeObject({{"p_room", self->m_room}, {"p_objects", matjson::Value::array()}, {"p_label", "sala vazia"}}),
                "Sala apagada");
    });
}

void CollabRoomPopup::onShowBans(CCObject*) { m_view = View::Bans; m_banPage = 0; render(); }
void CollabRoomPopup::onShowLog(CCObject*) { m_view = View::Log; render(); }
void CollabRoomPopup::onBack(CCObject*) { m_view = View::Main; render(); }
void CollabRoomPopup::onBanPage(CCObject* sender) { m_banPage += sender->getTag(); render(); }

void CollabRoomPopup::onUnban(CCObject* sender) {
    auto rows = m_info["bans"].isArray() ? m_info["bans"].asArray().unwrap() : std::vector<matjson::Value>{};
    auto index = static_cast<size_t>(sender->getTag());
    if (index >= rows.size()) return;
    run("geobr_collab_unban", matjson::makeObject({{"p_room", m_room}, {"p_user", jsonutil::stringOr(rows[index], "user_id")}}), "Desbanido");
}

// ---------------------------------------------------------- member actions
CollabMemberPopup* CollabMemberPopup::create(std::string room, matjson::Value member, int myRank, Done done) {
    auto p = new CollabMemberPopup;
    if (p->init(std::move(room), std::move(member), myRank, std::move(done))) { p->autorelease(); return p; }
    delete p;
    return nullptr;
}

bool CollabMemberPopup::init(std::string room, matjson::Value member, int myRank, Done done) {
    constexpr float w = 380.f, h = 260.f;
    if (!Popup::init(w, h)) return false;
    visual::fitPopup(m_mainLayer, {w, h});
    visual::stylePopup(m_mainLayer, m_bgSprite, {w, h}, {36, 52, 96});
    m_room = std::move(room);
    m_member = std::move(member);
    m_myRank = myRank;
    m_done = std::move(done);
    m_user = jsonutil::stringOr(m_member, "user_id");
    m_name = jsonutil::stringOr(m_member, "display_name", "Jogador");
    setTitle(m_name, "goldFont.fnt", .6f, 16);
    setID("geobr-collab-member");
    bool muted = jsonutil::boolOr(m_member, "muted", false);
    bool viewer = jsonutil::stringOr(m_member, "role") == "viewer";
    bool admin = jsonutil::boolOr(m_member, "can_save", false);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 2);

    label(m_mainLayer, "Reverter as edicoes dele", {w / 2, h - 50}, .34f, 300, ui::palette::TextGold);
    int minutes[] = {10, 60, 1440};
    char const* names[] = {"Ultimos 10 min", "Ultima 1 hora", "Ultimas 24 h"};
    for (int k = 0; k < 3; ++k) button(menu, names[k], {w / 2 + (k - 1) * 112.f, h - 76}, 104, "GJ_button_05.png", this, menu_selector(CollabMemberPopup::onRevert), minutes[k]);

    label(m_mainLayer, "Moderar", {w / 2, h - 108}, .34f, 300, ui::palette::TextGold);
    button(menu, "Expulsar", {w / 2 - 70, h - 134}, 120, "GJ_button_06.png", this, menu_selector(CollabMemberPopup::onKick));
    button(menu, "Banir", {w / 2 + 70, h - 134}, 120, "GJ_button_06.png", this, menu_selector(CollabMemberPopup::onBan));
    button(menu, muted ? "Liberar chat" : "Silenciar chat", {w / 2 - 70, h - 166}, 120, "GJ_button_02.png", this, menu_selector(CollabMemberPopup::onMute));
    button(menu, viewer ? "Liberar edicao" : "So ver (sem editar)", {w / 2 + 70, h - 166}, 120, "GJ_button_02.png", this, menu_selector(CollabMemberPopup::onViewer));
    if (m_myRank >= 3 && !viewer)
        button(menu, admin ? "Tirar ADM" : "Dar ADM", {w / 2 - 70, h - 198}, 120, admin ? "GJ_button_06.png" : "GJ_button_01.png", this, menu_selector(CollabMemberPopup::onAdmin));
    button(menu, "Ver perfil", {m_myRank >= 3 && !viewer ? w / 2 + 70 : w / 2, h - 198}, 120, "GJ_button_04.png", this, menu_selector(CollabMemberPopup::onProfile));

    m_status = ui::makeLabel(fmt::format("{}{}", viewer ? "So ver" : admin ? "ADM" : "Editor", muted ? "  |  silenciado no chat" : ""), "chatFont.fnt", .38f, {174, 209, 237});
    m_status->setPosition({w / 2, 16});
    m_mainLayer->addChild(m_status, 5);
    return true;
}

void CollabMemberPopup::run(std::string fn, matjson::Value args, std::string done, bool close) {
    if (m_busy) return;
    m_busy = true;
    m_status->setString("Aguarde...");
    WeakRef<CollabMemberPopup> weak = this;
    m_action.rpc(fn, std::move(args), [weak, done, close](auto r) {
        auto self = weak.lock();
        if (!self) return;
        self->m_busy = false;
        if (!r.success) { self->m_status->setString(r.error.c_str()); self->m_status->limitLabelWidth(350, .38f, .2f); return; }
        std::string text = r.value.contains("reverted")
            ? fmt::format("{}: {} objetos{}", done, jsonutil::integerOr(r.value, "reverted"),
                jsonutil::integerOr(r.value, "skipped") > 0 ? fmt::format(" ({} mexidos depois por outros)", jsonutil::integerOr(r.value, "skipped")) : "")
            : done;
        Notification::create(text, NotificationIcon::Success, 2.f)->show();
        if (self->m_done) self->m_done();
        if (close) self->onClose(nullptr);
        else { self->m_status->setString(text.c_str()); self->m_status->limitLabelWidth(350, .38f, .2f); }
    });
}

void CollabMemberPopup::confirm(std::string title, std::string text, std::string yes, std::function<void()> action) {
    createQuickPopup(title.c_str(), text, "Cancelar", yes.c_str(), [action](auto, bool ok) { if (ok) action(); });
}

void CollabMemberPopup::onRevert(CCObject* sender) {
    int minutes = sender->getTag();
    WeakRef<CollabMemberPopup> weak = this;
    confirm("Reverter edicoes", fmt::format("Desfazer o que <cy>{}</c> fez {}? O que outra pessoa mexeu depois fica.", m_name,
        minutes >= 1440 ? "nas ultimas 24 horas" : minutes >= 60 ? "na ultima hora" : fmt::format("nos ultimos {} minutos", minutes)), "Reverter", [weak, minutes] {
        if (auto self = weak.lock())
            self->run("geobr_collab_revert", matjson::makeObject({{"p_room", self->m_room}, {"p_user", self->m_user}, {"p_minutes", minutes}}), "Edicoes revertidas", false);
    });
}

void CollabMemberPopup::onKick(CCObject*) {
    WeakRef<CollabMemberPopup> weak = this;
    confirm("Expulsar", fmt::format("Expulsar <cy>{}</c> da sala? Ele pode voltar com o codigo (troque o codigo ou bana para impedir).", m_name), "Expulsar", [weak] {
        if (auto self = weak.lock())
            self->run("geobr_collab_kick", matjson::makeObject({{"p_room", self->m_room}, {"p_user", self->m_user}, {"p_ban", false}}), "Expulso da sala", true);
    });
}

void CollabMemberPopup::onBan(CCObject*) {
    WeakRef<CollabMemberPopup> weak = this;
    confirm("Banir", fmt::format("Banir <cr>{}</c>? Ele sai agora e nao entra mais nesta sala.", m_name), "Banir", [weak] {
        if (auto self = weak.lock())
            self->run("geobr_collab_kick", matjson::makeObject({{"p_room", self->m_room}, {"p_user", self->m_user}, {"p_ban", true}}), "Banido da sala", true);
    });
}

void CollabMemberPopup::onMute(CCObject*) {
    bool muted = jsonutil::boolOr(m_member, "muted", false);
    run("geobr_collab_mute", matjson::makeObject({{"p_room", m_room}, {"p_user", m_user}, {"p_on", !muted}}), muted ? "Chat liberado" : "Silenciado no chat", true);
}

void CollabMemberPopup::onViewer(CCObject*) {
    bool viewer = jsonutil::stringOr(m_member, "role") == "viewer";
    run("geobr_collab_set_viewer", matjson::makeObject({{"p_room", m_room}, {"p_user", m_user}, {"p_on", !viewer}}), viewer ? "Edicao liberada" : "Agora so ve a sala", true);
}

void CollabMemberPopup::onAdmin(CCObject*) {
    bool admin = jsonutil::boolOr(m_member, "can_save", false);
    run("geobr_collab_set_saver", matjson::makeObject({{"p_room", m_room}, {"p_user", m_user}, {"p_on", !admin}}), admin ? "ADM removido" : "Agora e ADM", true);
}

void CollabMemberPopup::onProfile(CCObject*) {
    if (auto p = PlayerCardPopup::create(m_member)) p->show();
}

} // namespace geobr
