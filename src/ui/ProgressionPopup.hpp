#pragma once
#include "../services/CloudService.hpp"
#include <Geode/ui/Popup.hpp>
namespace geobr {
void showCrateBundle(matjson::Value const&,int);
class ProgressionPopup final:public geode::Popup {
 CloudRequest m_request;CCNode* m_view=nullptr;cocos2d::CCLabelBMFont* m_status=nullptr;
 matjson::Value m_state,m_leaderboard;int m_tab=0,m_revealRarity=0;bool m_busy=false;std::vector<std::string> m_crates;
 bool init(int);void render();void onTab(CCObject*);void onClaim(CCObject*);void onOpen(CCObject*);void onInfo(CCObject*);void onRefresh(CCObject*);void finish(ApiJsonResult);
public:static ProgressionPopup* create(int tab=0);
};
}
