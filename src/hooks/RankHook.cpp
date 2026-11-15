#include "../services/RankService.hpp"
#include "../ui/UIHelpers.hpp"
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelCell.hpp>
using namespace geode::prelude;
namespace {
CCPoint pointIn(CCNode* root,CCNode* n){return root->convertToNodeSpace(n->convertToWorldSpace(n->getContentSize()/2));}
void badge(CCNode* root,int rank,CCPoint at,float scale){
 if(auto old=root->getChildByID("geobr-top-label"))old->removeFromParent();if(rank<=0)return;
 auto label=geobr::ui::makeLabel(fmt::format("TOP {}",rank),"bigFont.fnt",scale,{255,226,93});label->limitLabelWidth(66,scale,.15f);
 auto panel=geobr::ui::makePanel({label->getScaledContentWidth()+10,label->getScaledContentHeight()+4},{31,26,54},245);panel->setID("geobr-top-label");panel->setPosition(at);label->setPosition(panel->getContentSize()/2);panel->addChild(label);root->addChild(panel,30);
}
}
class $modify(GeoBRRankInfo,LevelInfoLayer){
 struct Fields{CCPoint star,icon;std::vector<std::pair<Ref<CCNode>,CCPoint>> coins;int rank=-1;};
 bool init(GJGameLevel* level,bool challenge){
  if(!LevelInfoLayer::init(level,challenge))return false;
  if(m_starsLabel)m_fields->star=m_starsLabel->getPosition();if(m_starsIcon)m_fields->icon=m_starsIcon->getPosition();
  if(m_coins)for(auto c:CCArrayExt<CCNode*>(m_coins))m_fields->coins.emplace_back(c,c->getPosition());
  updateTop(0);schedule(schedule_selector(GeoBRRankInfo::updateTop),1);return true;
 }
 void updateTop(float){
  if(!m_level||!m_starsLabel)return;int rank=geobr::RankService::get()->position(m_level->m_levelID.value());if(rank==m_fields->rank)return;m_fields->rank=rank;
  // The native star row is a reliable slot below the entire difficulty graphic,
  // including two-line Extreme Demon. Reserve that slot; shift stats, never guess text bounds.
  m_starsLabel->setPosition(m_fields->star);auto slot=pointIn(this,m_starsLabel);
  if(m_starsIcon){m_starsIcon->setPosition(m_fields->icon);auto center=pointIn(this,m_starsIcon);slot.x=(slot.x+center.x)/2;}
  float shift=rank?15.f:0;m_starsLabel->setPosition(m_fields->star-CCPoint{0,shift});if(m_starsIcon)m_starsIcon->setPosition(m_fields->icon-CCPoint{0,shift});
  for(auto const& [c,pos]:m_fields->coins)c->setPosition(pos-CCPoint{0,shift});
  badge(this,rank,slot,.24f);
 }
};
class $modify(GeoBRRankCell,LevelCell){
 struct Fields{Ref<GJGameLevel> level;int rank=-1;};
 void loadFromLevel(GJGameLevel* level){if(auto old=getChildByID("geobr-top-label"))old->removeFromParent();LevelCell::loadFromLevel(level);m_fields->level=level;m_fields->rank=-1;updateTop(0);schedule(schedule_selector(GeoBRRankCell::updateTop),1);}
 void updateTop(float){if(!m_fields->level)return;int rank=geobr::RankService::get()->position(m_fields->level->m_levelID.value());if(rank==m_fields->rank)return;m_fields->rank=rank;
  // Separate right-hand slot above View. Difficulty/stars remain entirely native.
  auto size=getContentSize();bool compact=size.height<60;badge(this,rank,{size.width-55,size.height-(compact?6.f:20.f)},compact?.15f:.21f);
 }
};
