#pragma once
#include "CloudService.hpp"
#include <Geode/Geode.hpp>
namespace geobr {
inline constexpr char ClientVersion[]="1.13.0";
class AccessService final:public cocos2d::CCNode {
 CloudRequest m_request;bool m_started=false,m_pending=false;matjson::Value m_status=matjson::Value::object();
 void poll(float);
public:static AccessService* get();void start();void refresh();bool allowed(bool notify=true)const;bool canChat()const;bool canDM()const;std::string reason()const;
};}
