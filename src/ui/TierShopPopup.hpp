#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
namespace geobr {
class TierShopPopup final:public geode::Popup {
 CloudRequest m_request;cocos2d::CCNode* m_view=nullptr;cocos2d::CCLabelBMFont* m_status=nullptr;
 matjson::Value m_state;std::vector<matjson::Value> m_items;int m_category=0;int m_page=0;bool m_busy=false;
 bool init(int,bool);void refresh();void finish(ApiJsonResult);void render();void retryOrder();
 void onPreview(cocos2d::CCObject*);void onOwned(cocos2d::CCObject*);bool m_ownedOnly=false;
 void onCategory(cocos2d::CCObject*);void onPage(cocos2d::CCObject*);void onItem(cocos2d::CCObject*);void onRetry(cocos2d::CCObject*);void buy(matjson::Value const&);
public:static TierShopPopup* create(int category=0,bool ownedOnly=false);
};}
