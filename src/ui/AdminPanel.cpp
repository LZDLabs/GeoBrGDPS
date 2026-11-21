#include "AdminPanel.hpp"
#include "AdminForm.hpp"
#include "AdminSafetyPopup.hpp"
#include "SeasonBuilderPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../models/JsonModelUtils.hpp"
#include <Geode/ui/GeodeUI.hpp>
#include <array>
using namespace geode::prelude;
namespace geobr {
std::string adminTable(AdminEntity e){static const std::array<std::string,10> tables{"geobr_news","geobr_announcements","geobr_pass_seasons","geobr_pass_rewards","geobr_quest_steps","geobr_main_levels","geobr_shop_items","geobr_level_ranks","geobr_account_links","geobr_natal_levels"};return tables[static_cast<int>(e)];}
std::string adminTitle(AdminEntity e){static const std::array<std::string,10> titles{"Noticias","Avisos","Temporadas","Premios","Quests","Main Levels","Tier Shop","Ranking","Contas","Natal"};return titles[static_cast<int>(e)];}
AdminLogin* AdminLogin::create(){auto p=new AdminLogin;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
bool AdminLogin::init(){
    if(!Popup::init(320,185))return false;visual::fitPopup(m_mainLayer,{320,185});visual::stylePopup(m_mainLayer,m_bgSprite,{320,185},{53,39,78});setTitle("Acesso ADM","goldFont.fnt",.7f,15);setID("geobr-admin-login");
    auto hint=ui::makeLabel("Digite a senha do painel","chatFont.fnt",.6f);hint->setPosition({160,130});m_mainLayer->addChild(hint);
    m_password=TextInput::create(260,"Senha");m_password->setCommonFilter(CommonFilter::Any);m_password->setMaxCharCount(128);m_password->setPasswordMode(true);m_password->setPosition({160,99});m_mainLayer->addChild(m_password);
    auto menu=CCMenu::create();menu->setPosition({160,58});m_submit=ui::makeTextButton("Entrar",120,"GJ_button_01.png",this,menu_selector(AdminLogin::onSubmit),.4f,30);menu->addChild(m_submit);m_mainLayer->addChild(menu);
    m_status=ui::makeLabel("","chatFont.fnt",.48f,{255,195,137});m_status->setPosition({160,24});m_mainLayer->addChild(m_status);return true;
}
void AdminLogin::onSubmit(CCObject*){
    std::string password=m_password->getString();if(password.empty())return;m_submit->setEnabled(false);m_password->setString("");m_status->setString("Verificando...");
    m_request.rpc("geobr_admin_login",matjson::makeObject({{"p_password",password}}),[this](auto result){
        m_submit->setEnabled(true);
        if(result.success&&jsonutil::boolOr(result.value,"ok")){if(auto panel=AdminPanel::create())panel->show();onClose(nullptr);return;}
        auto error=result.success?jsonutil::stringOr(result.value,"error","Acesso negado."):result.error;m_status->setString(error.c_str());m_status->limitLabelWidth(285,.48f,.25f);
    });
}
AdminPanel* AdminPanel::create(){auto p=new AdminPanel;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
bool AdminPanel::init(){
    if(!Popup::init(520,330))return false;visual::fitPopup(m_mainLayer,{520,330});visual::stylePopup(m_mainLayer,m_bgSprite,{520,330},{53,39,78});auto bg=visual::theme({506,316},"cosmos");bg->setPosition({7,7});m_mainLayer->addChild(bg,-1);setID("geobr-admin-panel");setTitle("Painel ADM - GeoBR","goldFont.fnt",.7f,17);
    auto menu=CCMenu::create();menu->setPosition({0,0});m_mainLayer->addChild(menu,5);
    for(int i=0;i<10;++i){auto button=ui::makeTextButton(adminTitle(static_cast<AdminEntity>(i)),106,i==9?"GJ_button_06.png":"GJ_button_04.png",this,menu_selector(AdminPanel::onTab),.28f,24);button->setPosition({73.f,267.f-i*24.f});button->setTag(i);menu->addChild(button);}
    m_list=ScrollLayer::create({355,196},true,true);m_list->setPosition({145,72});m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(5));m_mainLayer->addChild(m_list);
    auto add=ui::makeTextButton("Novo",85,"GJ_button_01.png",this,menu_selector(AdminPanel::onNew),.36f,29);add->setPosition({185,38});menu->addChild(add);
    for(int i=0;i<2;++i){auto b=ui::makeTextButton(i?">":"<",40,"GJ_button_04.png",this,menu_selector(AdminPanel::onPage),.45f,28);b->setPosition({260.f+i*48.f,38});b->setTag(i?1:-1);menu->addChild(b);}
    auto refresh=ui::makeTextButton("Atualizar lista",126,"GJ_button_01.png",this,menu_selector(AdminPanel::onRefresh),.31f,29);refresh->setPosition({432,38});menu->addChild(refresh);
    m_status=ui::makeLabel("","chatFont.fnt",.43f,{185,232,255});m_status->setPosition({320,16});m_mainLayer->addChild(m_status);auto builder=ui::makeTextButton("Gerar passe",92,"GJ_button_01.png",this,menu_selector(AdminPanel::onBuilder),.28f,27);builder->setPosition({300,286});menu->addChild(builder);auto safety=ui::makeTextButton("Seguranca",124,"GJ_button_06.png",this,menu_selector(AdminPanel::onSafety),.29f,26);safety->setPosition({438,286});menu->addChild(safety);load();return true;
}
void AdminPanel::status(std::string const& text){m_status->setString(text.c_str());m_status->limitLabelWidth(350,.43f,.20f);}
void AdminPanel::load(){
    m_busy=true;status("Carregando...");m_request.call("GET",fmt::format("/rest/v1/{}?order=id.desc&limit=20&offset={}",adminTable(m_entity),m_page*20),{},[this](auto result){
        m_busy=false;if(!result.success||!result.value.isArray()){status(result.error.empty()?"Resposta invalida.":result.error);return;}
        m_rows=result.value.asArray().unwrap();render();status(fmt::format("{} - pagina {}. Sessao ADM dura 20 minutos.",adminTitle(m_entity),m_page+1));
    });
}
void AdminPanel::render(){
    ui::clearScroll(m_list);
    if(m_rows.empty()){auto empty=ui::makeLabel("Sem registros. Toque Novo para adicionar.","chatFont.fnt",.6f,{196,222,255});empty->limitLabelWidth(325,.5f,.25f);empty->setPosition({177,95});m_list->m_contentLayer->addChild(empty);return;}
    for(size_t i=0;i<m_rows.size();++i){auto const& row=m_rows[i];
        auto node=CCNode::create();node->setContentSize({347,58});node->setAnchorPoint({.5f,.5f});auto bg=ui::makePanel({347,58},{29,49,83},245);bg->setPosition({173.5f,29});node->addChild(bg);
        auto name=jsonutil::stringOr(row,"title",jsonutil::stringOr(row,"name"));if(name.empty())name=fmt::format("Temp. {} / Etapa {} / {}",jsonutil::integerOr(row,"season_id"),jsonutil::integerOr(row,"tier"),jsonutil::stringOr(row,"kind"));
        if(m_entity==AdminEntity::Ranks)name=fmt::format("TOP {} | Nivel ID {}",jsonutil::integerOr(row,"position"),jsonutil::integerOr(row,"level_id"));
        auto label=ui::makeLabel(fmt::format("#{} {}",jsonutil::stringOr(row,"id",std::to_string(jsonutil::integerOr(row,"id"))),name),"bigFont.fnt",.34f,{255,226,115});label->setAnchorPoint({0,.5f});label->setPosition({9,44});label->limitLabelWidth(325,.31f,.19f);node->addChild(label);
        auto sub=ui::makeLabel(row.contains("published")?(jsonutil::boolOr(row,"published")?"Publicado para todos":"Rascunho"):"Configuracao da temporada","chatFont.fnt",.33f);sub->setAnchorPoint({0,.5f});sub->setPosition({9,15});node->addChild(sub);
        auto menu=CCMenu::create();menu->setPosition({0,0});auto edit=ui::makeTextButton("Editar",65,"GJ_button_01.png",this,menu_selector(AdminPanel::onEdit),.24f,21);edit->setPosition({225,16});edit->setTag(i);menu->addChild(edit);
        auto remove=ui::makeTextButton("Remover",70,"GJ_button_06.png",this,menu_selector(AdminPanel::onRemove),.23f,21);remove->setPosition({306,16});remove->setTag(i);menu->addChild(remove);node->addChild(menu);m_list->m_contentLayer->addChild(node);
    }
    m_list->m_contentLayer->updateLayout();m_list->scrollToTop();
}
void AdminPanel::onSafety(CCObject*){if(auto p=AdminSafetyPopup::create())p->show();}
void AdminPanel::onBuilder(CCObject*){if(auto p=SeasonBuilderPopup::create())p->show();}
void AdminPanel::onTab(CCObject* sender){if(m_busy)return;m_entity=static_cast<AdminEntity>(sender->getTag());m_page=0;load();}
void AdminPanel::onNew(CCObject*){if(!m_busy)if(auto form=AdminForm::create(m_entity))form->show();}
void AdminPanel::onEdit(CCObject* sender){auto i=sender->getTag();if(!m_busy&&i>=0&&i<m_rows.size())if(auto form=AdminForm::create(m_entity,m_rows[i]))form->show();}
void AdminPanel::onPage(CCObject* sender){if(m_busy)return;auto direction=sender->getTag();if(direction<0&&m_page==0)return;if(direction>0&&m_rows.size()<20)return;m_page+=direction;load();}
void AdminPanel::onRefresh(CCObject*){if(!m_busy)load();}
void AdminPanel::onRemove(CCObject* sender){
    auto index=sender->getTag();if(m_busy||index<0||index>=m_rows.size())return;
    auto id=jsonutil::stringOr(m_rows[index],"id",std::to_string(jsonutil::integerOr(m_rows[index],"id")));auto entity=m_entity;WeakRef<AdminPanel> weak=this;
    createQuickPopup("Remover globalmente?","Esta alteracao sera aplicada para todos os jogadores. O arquivo original de um Main Level permanece guardado.","Cancelar","Remover",[weak,id,entity](auto,bool yes){
        auto self=weak.lock();if(!yes||!self)return;self->m_busy=true;
        self->m_request.call("DELETE",fmt::format("/rest/v1/{}?id=eq.{}",adminTable(entity),id),{},[weak](auto result){
            auto self=weak.lock();if(!self)return;self->m_busy=false;
            if(!result.success){self->status(result.error);return;}
            if(result.value.isArray()&&result.value.asArray().unwrap().empty()){self->status("Nao removido. Reabra o ADM para renovar a sessao.");return;}
            self->load();
        });
    });
}
void AdminPanel::onClose(CCObject* sender){
    m_request.cancel();static CloudRequest logout;logout.rpc("geobr_admin_logout",matjson::Value::object(),[](auto){});Popup::onClose(sender);
}
}
