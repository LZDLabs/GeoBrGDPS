#include "AdminSafetyPopup.hpp"
#include "UIHelpers.hpp"
#include "GeoBRVisuals.hpp"
#include "../services/AccessService.hpp"
#include "../models/JsonModelUtils.hpp"
#include <Geode/ui/TextInput.hpp>
#include <charconv>
using namespace geode::prelude;
namespace geobr { namespace {
void text(CCNode* n,std::string const& s,CCPoint p,float k,float w,ccColor3B color={255,255,255}){auto l=ui::makeLabel(s,"bigFont.fnt",k,color);l->setPosition(p);l->limitLabelWidth(w,k,.17f);n->addChild(l);}
class SafetyForm final:public Popup {
 CloudRequest request;matjson::Value target;TextInput *reason=nullptr,*duration=nullptr,*version=nullptr;CCLabelBMFont *status=nullptr,*choice=nullptr;bool releaseMode=false,busy=false,enabled=true;int kind=0;
 bool init(matjson::Value const& row,bool isRelease){
  if(!Popup::init(420,284))return false;visual::fitPopup(m_mainLayer,{420,284});target=row;releaseMode=isRelease;visual::stylePopup(m_mainLayer,m_bgSprite,{420,284},{44,38,68});setTitle(releaseMode?"Controle de versao":"Moderar jogador");
  text(m_mainLayer,releaseMode?"Ativar ou suspender uma versao":jsonutil::stringOr(row,"display_name","Jogador"),{210,236},.36f,365,{255,223,128});
  if(releaseMode){version=TextInput::create(350,"Ex.: 1.9.0-beta.4","chatFont.fnt");version->setCommonFilter(CommonFilter::Any);version->setMaxCharCount(40);version->setString(jsonutil::stringOr(row,"version"));version->setPosition({210,200});m_mainLayer->addChild(version);enabled=jsonutil::boolOr(row,"enabled",true);}
  else {text(m_mainLayer,jsonutil::stringOr(row,"user_id"),{210,211},.19f,365);duration=TextInput::create(350,"Prazo em minutos (0 = permanente)","chatFont.fnt");duration->setCommonFilter(CommonFilter::Uint);duration->setString("60");duration->setMaxCharCount(6);duration->setPosition({210,152});m_mainLayer->addChild(duration);}
  auto menu=CCMenu::create();menu->setPosition({0,0});m_mainLayer->addChild(menu);
  choice=ui::makeLabel("","bigFont.fnt",.32f);auto btn=CCMenuItemSpriteExtra::create(choice,this,menu_selector(SafetyForm::onChoice));btn->setPosition({210,releaseMode?160.f:182.f});menu->addChild(btn);updateChoice();
  reason=TextInput::create(350,releaseMode?"Mensagem para atualizar":"Motivo obrigatorio","chatFont.fnt");reason->setCommonFilter(CommonFilter::Any);reason->setMaxCharCount(200);reason->setString(jsonutil::stringOr(row,"message",releaseMode?"Atualize o GeoBR pelo canal oficial.":""));reason->setPosition({210,112});m_mainLayer->addChild(reason);
  auto save=ui::makeTextButton("Revisar e aplicar",230,"GJ_button_01.png",this,menu_selector(SafetyForm::onSave),.35f,32);save->setPosition({210,63});menu->addChild(save);
  status=ui::makeLabel(releaseMode?"Suspende recursos GeoBR dessa versao.":"Mute bloqueia envios no chat e nas DMs.","chatFont.fnt",.37f,{190,214,239});status->setPosition({210,25});status->limitLabelWidth(370,.37f,.2f);m_mainLayer->addChild(status);return true;
 }
 void updateChoice(){const char* names[]={"Mute temporario","Ban do chat geral","Ban do mod"};choice->setString(releaseMode?(enabled?"Versao: LIBERADA (toque para mudar)":"Versao: BLOQUEADA (toque para mudar)"):names[kind]);choice->limitLabelWidth(350,.32f,.21f);choice->setColor(releaseMode&&!enabled?ccColor3B{255,144,130}:ccColor3B{126,238,218});}
 void onChoice(CCObject*){if(busy)return;if(releaseMode)enabled=!enabled;else kind=(kind+1)%3;updateChoice();}
 void onSave(CCObject*){
  if(busy)return;std::string why=reason->getString();if(why.size()<3){status->setString("Escreva um motivo com pelo menos 3 caracteres.");return;}
  matjson::Value data;std::string action,summary;
  if(releaseMode){std::string v=version->getString();if(v.empty())return;action="save_version";data=matjson::makeObject({{"version",v},{"enabled",enabled},{"message",why}});summary=fmt::format("Versao <cy>{}</c>\n{}\n{}",v,enabled?"Liberar recursos GeoBR":"Suspender recursos GeoBR",why);}
  else {int minutes=0;std::string raw=duration->getString();auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),minutes);if(raw.empty()||parsed.ec!=std::errc{}||parsed.ptr!=raw.data()+raw.size()){status->setString("Informe um prazo em minutos.");return;}if(minutes<0||minutes>525600||(!kind&&!minutes)){status->setString("Mute exige prazo maior que zero.");return;}action="punish";data=matjson::makeObject({{"user_id",jsonutil::stringOr(target,"user_id")},{"kind",kind==0?"mute":kind==1?"chat":"mod"},{"minutes",minutes},{"reason",why}});summary=fmt::format("{}\nPrazo: {}\nMotivo: {}",jsonutil::stringOr(target,"display_name"),minutes?std::to_string(minutes)+" minutos":"Permanente",why);}
  WeakRef<SafetyForm> weak=this;createQuickPopup("Confirmar moderacao",summary,"Cancelar","Aplicar",[weak,data,action](auto,bool yes){auto self=weak.lock();if(!self||!yes)return;self->busy=true;self->request.rpc("geobr_moderation",matjson::makeObject({{"p_action",action},{"p_data",data}}),[weak](auto r){if(auto s=weak.lock()){s->busy=false;s->status->setString(r.success?"Aplicado. Atualize a lista do painel.":r.error.c_str());s->status->limitLabelWidth(370,.37f,.2f);AccessService::get()->refresh();}});});
 }
public:static SafetyForm* create(matjson::Value const& row,bool releaseMode){auto p=new SafetyForm;if(p->init(row,releaseMode)){p->autorelease();return p;}delete p;return nullptr;}
};}
AdminSafetyPopup* AdminSafetyPopup::create(){auto p=new AdminSafetyPopup;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
bool AdminSafetyPopup::init(){
 if(!Popup::init(520,320))return false;visual::fitPopup(m_mainLayer,{520,320});visual::stylePopup(m_mainLayer,m_bgSprite,{520,320},{40,35,65});setTitle("Seguranca GeoBR");auto menu=CCMenu::create();menu->setPosition({0,0});m_mainLayer->addChild(menu);
 const char* tabs[]={"Jogadores","Punicoes","Versoes","Historico"};for(int i=0;i<4;++i){auto b=ui::makeTextButton(tabs[i],112,"GJ_button_04.png",this,menu_selector(AdminSafetyPopup::onTab),.28f,28);b->setPosition({77.f+122*i,265});b->setTag(i);menu->addChild(b);}
 m_list=ScrollLayer::create({476,190},true,true);m_list->setPosition({22,60});m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(5));m_mainLayer->addChild(m_list);
 auto refresh=ui::makeTextButton("Atualizar",140,"GJ_button_01.png",this,menu_selector(AdminSafetyPopup::onRefresh),.31f,28);refresh->setPosition({125,34});menu->addChild(refresh);auto add=ui::makeTextButton("Adicionar versao",210,"GJ_button_04.png",this,menu_selector(AdminSafetyPopup::onNew),.29f,28);add->setPosition({362,34});menu->addChild(add);
 m_status=ui::makeLabel("","chatFont.fnt",.35f);m_status->setPosition({260,12});m_mainLayer->addChild(m_status);load();return true;
}
void AdminSafetyPopup::load(){if(m_busy)return;m_busy=true;auto action=m_tab==0?"players":m_tab==2?"versions":"list";m_status->setString("Carregando...");m_request.rpc("geobr_moderation",matjson::makeObject({{"p_action",action}}),[this](auto r){m_busy=false;if(!r.success){m_status->setString(r.error.c_str());m_status->limitLabelWidth(460,.35f,.2f);return;}m_rows=r.value;if(m_tab==1||m_tab==3){auto list=r.value.get(m_tab==1?"restrictions":"log");m_rows=list&&list.unwrap().isArray()?list.unwrap():matjson::Value::array();}render();m_status->setString("Alteracoes ficam registradas. Toque na linha para gerenciar.");});}
void AdminSafetyPopup::render(){ui::clearScroll(m_list);if(!m_rows.isArray())return;auto rows=m_rows.asArray().unwrap();for(int i=0;i<rows.size();++i){auto const& row=rows[i];auto box=ui::makePanel({464,58},{28,38,65},255);
 std::string title,sub;if(!m_tab){title=jsonutil::stringOr(row,"display_name");sub=jsonutil::stringOr(row,"user_id");}else if(m_tab==1){title=jsonutil::stringOr(row,"display_name")+" | "+jsonutil::stringOr(row,"kind");sub=jsonutil::stringOr(row,"reason")+" | "+(jsonutil::stringOr(row,"revoked_at").empty()?jsonutil::stringOr(row,"expires_at","Permanente"):"REVOGADO");}else if(m_tab==2){title=jsonutil::stringOr(row,"version")+(jsonutil::boolOr(row,"enabled")?" | LIBERADA":" | BLOQUEADA");sub=jsonutil::stringOr(row,"message");}else{title=jsonutil::stringOr(row,"action");sub=jsonutil::stringOr(row,"created_at")+" | ADM "+jsonutil::stringOr(row,"actor");}
 text(box,title,{232,39},.31f,440,{255,226,143});text(box,sub,{232,17},.21f,440,{180,211,236});auto item=CCMenuItemSpriteExtra::create(box,this,menu_selector(AdminSafetyPopup::onRow));item->setTag(i);auto menu=CCMenu::create();menu->setPosition({232,29});menu->addChild(item);auto node=CCNode::create();node->setContentSize({464,58});node->setAnchorPoint({.5f,.5f});node->addChild(menu);m_list->m_contentLayer->addChild(node);
 }m_list->m_contentLayer->updateLayout();m_list->scrollToTop();if(rows.empty())m_status->setString("Nenhum registro.");}
void AdminSafetyPopup::onTab(CCObject* s){if(m_busy)return;m_tab=s->getTag();load();}
void AdminSafetyPopup::onRefresh(CCObject*){load();}
void AdminSafetyPopup::onNew(CCObject*){if(auto p=SafetyForm::create(matjson::Value::object(),true))p->show();}
void AdminSafetyPopup::onRow(CCObject* sender){if(m_busy||!m_rows.isArray())return;auto rows=m_rows.asArray().unwrap();int i=sender->getTag();if(i<0||i>=rows.size())return;auto row=rows[i];if(m_tab==0||m_tab==2){if(auto p=SafetyForm::create(row,m_tab==2))p->show();return;}if(m_tab==3){FLAlertLayer::create(nullptr,"Registro de moderacao",row.dump(),"OK",nullptr,390,true,220,.6f)->show();return;}if(m_tab!=1)return;
 WeakRef<AdminSafetyPopup> weak=this;createQuickPopup("Revogar punicao?",jsonutil::stringOr(row,"reason"),"Cancelar","Revogar",[weak,row](auto,bool yes){auto s=weak.lock();if(!s||!yes)return;s->m_busy=true;s->m_request.rpc("geobr_moderation",matjson::makeObject({{"p_action","revoke"},{"p_data",matjson::makeObject({{"id",jsonutil::integerOr(row,"id")}})}}),[weak](auto r){if(auto p=weak.lock()){p->m_busy=false;if(r.success)p->load();else p->m_status->setString(r.error.c_str());}});});
}
}
