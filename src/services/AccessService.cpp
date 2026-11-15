#include "AccessService.hpp"
#include "../models/JsonModelUtils.hpp"
#include <Geode/ui/Notification.hpp>
#include <Geode/binding/PlayLayer.hpp>
using namespace geode::prelude;
namespace geobr {
AccessService* AccessService::get(){static auto s=[](){auto p=new AccessService;p->init();return p;}();return s;}
void AccessService::start(){if(m_started)return;m_started=true;m_status=Mod::get()->getSavedValue<matjson::Value>("access-status",matjson::Value::object());CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(AccessService::poll),this,20,false);refresh();}
// A running level is never interrupted by a status change, so polling there
// only costs battery and data.
void AccessService::poll(float){if(PlayLayer::get())return;refresh();}
void AccessService::refresh(){if(m_pending)return;m_pending=true;m_request.rpc("geobr_access_status",matjson::makeObject({{"p_version",ClientVersion}}),[this](auto r){m_pending=false;if(!r.success)return;bool before=allowed(false);m_status=r.value;Mod::get()->setSavedValue("access-status",m_status);if(before&&!allowed(false))Notification::create(reason(),NotificationIcon::Warning,8)->show();});}
bool AccessService::allowed(bool notify)const{bool ok=jsonutil::boolOr(m_status,"allowed",true);if(!ok&&notify)FLAlertLayer::create("Acesso GeoBR",reason().c_str(),"OK")->show();return ok;}
bool AccessService::canChat()const{return allowed(false)&&jsonutil::boolOr(m_status,"chat_allowed",true);}
bool AccessService::canDM()const{return allowed(false)&&jsonutil::boolOr(m_status,"dm_allowed",true);}
std::string AccessService::reason()const{if(!jsonutil::boolOr(m_status,"allowed",true))return jsonutil::stringOr(m_status,"reason","Acesso suspenso. Consulte o ADM.");auto reason=jsonutil::stringOr(m_status,"chat_reason");auto until=jsonutil::stringOr(m_status,"chat_until");return reason.empty()?"Acesso liberado":reason+(until.empty()?" | Ate revisao do ADM":" | Ate "+until.substr(0,16)+" UTC");}
}
