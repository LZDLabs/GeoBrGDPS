#include "TierShopPopup.hpp"
#include "ProfilePreviewPopup.hpp"
#include "GeoBRVisuals.hpp"
#include "UIHelpers.hpp"
#include "../services/GeoPassService.hpp"
#include "../models/ShopOrder.hpp"
#include "../arrow/ArrowSkins.hpp"
#include <Geode/ui/TextArea.hpp>
#include <Geode/ui/Notification.hpp>
#include <algorithm>
using namespace geode::prelude;
namespace geobr {
namespace {
void label(CCNode* p,std::string const& s,CCPoint pos,float scale,float width,ccColor3B color={255,255,255}) {
 auto l=ui::makeLabel(s,"bigFont.fnt",scale,color);l->setPosition(pos);l->limitLabelWidth(width,scale,.21f);p->addChild(l);
}
matjson::Value pending(){return Mod::get()->getSavedValue<matjson::Value>("pending-shop-order",matjson::Value(nullptr));}
bool savePending(matjson::Value const& v){Mod::get()->setSavedValue("pending-shop-order",v);return Mod::get()->saveData().isOk();}
}
TierShopPopup* TierShopPopup::create(int c,bool ownedOnly){auto p=new TierShopPopup;if(p->init(c,ownedOnly)){p->autorelease();return p;}delete p;return nullptr;}
bool TierShopPopup::init(int c,bool ownedOnly){
 if(!Popup::init(550,320))return false;visual::fitPopup(m_mainLayer,{550,320});visual::stylePopup(m_mainLayer,m_bgSprite,{550,320},{53,39,78});auto win=CCDirector::sharedDirector()->getWinSize();auto backdrop=visual::theme({536,306},"cosmos");backdrop->setPosition({7,7});m_mainLayer->addChild(backdrop,-1);
 m_category=std::clamp(c,0,3);m_ownedOnly=ownedOnly;setID("geobr-tier-shop");
 m_status=ui::makeLabel("Carregando loja...","chatFont.fnt",.4f,{163,235,255});m_status->setPosition({232,16});m_mainLayer->addChild(m_status,5);
 m_state=GeoPassService::get()->state();render();
 // Beta 1.7 saved {} as its idle marker. It is not an outstanding purchase.
 if(isPendingShopOrder(pending()))retryOrder();else {savePending(matjson::Value(nullptr));refresh();}
 return true;
}
void TierShopPopup::refresh(){
 if(m_busy)return;m_busy=true;render();
 m_request.rpc("geobr_state_v2",matjson::Value::object(),[this](auto r){finish(std::move(r));});
}
void TierShopPopup::finish(ApiJsonResult r){
 m_busy=false;
 if(r.success){GeoPassService::get()->ingest(r.value);m_state=GeoPassService::get()->state();m_status->setString("");}
 else {m_status->setString(r.error.c_str());m_status->limitLabelWidth(389,.4f,.23f);}
 render();
}
void TierShopPopup::onOwned(CCObject*){if(m_busy)return;m_ownedOnly=!m_ownedOnly;m_page=0;render();}
void TierShopPopup::onPreview(CCObject* s){int i=s->getTag();if(i>=0&&i<m_items.size())if(auto p=ProfilePreviewPopup::create(jsonutil::stringOr(m_items[i],"style","default")))p->show();}
void TierShopPopup::onCategory(CCObject* s){if(m_busy)return;m_category=s->getTag();m_page=0;render();}
void TierShopPopup::onPage(CCObject* s){if(m_busy)return;m_page+=s->getTag();render();}
void TierShopPopup::onRetry(CCObject*){if(m_busy)return;if(isPendingShopOrder(pending()))retryOrder();else refresh();}
void TierShopPopup::render(){
 if(m_view)m_view->removeFromParent();m_view=CCNode::create();m_mainLayer->addChild(m_view);
 label(m_view,"Tier Shop",{143,292},.73f,226,{255,232,116});
 if(auto token=CCSprite::create("geobr-token.png"_spr)){token->setScale(26.f/token->getContentWidth());token->setPosition({399,292});m_view->addChild(token);}
 auto balance=jsonutil::integerOr(m_state,"tokens");label(m_view,ui::compactNumber(balance),{468,292},.5f,96,{255,225,88});
 auto menu=CCMenu::create();menu->setPosition({0,0});m_view->addChild(menu,3);const char* names[]={"Temas","Titulos","Skips","Arrows"};
 for(int i=0;i<4;++i){auto b=ui::makeTextButton(names[i],118,i==m_category?"GJ_button_01.png":"GJ_button_04.png",this,menu_selector(TierShopPopup::onCategory),.34f,27);b->setPosition({83.f+i*128,257});b->setTag(i);b->setEnabled(!m_busy);menu->addChild(b);}
 auto owned=ui::makeTextButton(m_ownedOnly?"Todos":"Colecao",110,"GJ_button_04.png",this,menu_selector(TierShopPopup::onOwned),.27f,23);owned->setPosition({80,17});menu->addChild(owned);
 auto retry=ui::makeTextButton("Atualizar",89,"GJ_button_04.png",this,menu_selector(TierShopPopup::onRetry),.28f,23);retry->setPosition({480,17});retry->setEnabled(!m_busy);menu->addChild(retry);
 m_items.clear();auto values=m_state.get("shop");
 if(values&&values.unwrap().isArray())for(auto const& item:values.unwrap().asArray().unwrap()){
  if(m_ownedOnly&&!jsonutil::boolOr(item,"owned"))continue;auto k=jsonutil::stringOr(item,"kind");if((m_category==0&&k=="theme")||(m_category==1&&k=="title")||(m_category==2&&(k=="tier_skip"||k=="quest_skip"))||(m_category==3&&k=="arrow"))m_items.push_back(item);
 }
 int pages=std::max(1,int((m_items.size()+1)/2));m_page=std::clamp(m_page,0,pages-1);
 if(m_items.empty())label(m_view,m_busy?"Carregando itens...":"Nenhum item nesta categoria",{275,158},.4f,449);
 for(int col=0;col<2;++col){size_t index=m_page*2+col;if(index>=m_items.size())break;auto const& item=m_items[index];float x=144.f+col*262;
  auto bg=ui::makePanel({244,180},{23,28,56},255);bg->setPosition({x,143});m_view->addChild(bg);
  auto kind=jsonutil::stringOr(item,"kind"),name=jsonutil::stringOr(item,"name");
  label(m_view,name,{x,218},.43f,223,{255,232,128});
  auto profile=m_state.get("profile");
  auto style=kind=="theme"?jsonutil::stringOr(item,"style"):profile?jsonutil::stringOr(profile.unwrap(),"theme_slug","default"):"default";
  auto preview=visual::theme({220,77},style);preview->setPosition({x-110,129});m_view->addChild(preview);
  if(kind=="arrow"){
   // Arrow skins preview with the player's own Col 1 / Col 2 / Glow.
   auto look=arrow::ArrowSettings::localAppearance();look.skin=arrow::skinFromItem(jsonutil::stringOr(item,"id"));
   if(look.skin>0)if(auto icon=arrow::ArrowIcon::create(look)){icon->setScale(1.7f);icon->setRotation(-45.f);icon->setPosition({110,38});preview->addChild(icon,3);}
  }
  else if(kind=="theme")label(preview,"GeoBR",{110,37},.59f,192,{255,255,255});
  else label(preview,kind=="title"?name:kind=="tier_skip"?"+1 ETAPA":"PROXIMA QUEST",{110,38},kind=="title"?.45f:.38f,195,{255,240,167});
  if(kind=="theme"){auto previewBtn=ui::makeTextButton("Experimentar",125,"GJ_button_04.png",this,menu_selector(TierShopPopup::onPreview),.23f,20);previewBtn->setPosition({x,139});previewBtn->setTag(index);menu->addChild(previewBtn);}
  auto desc=SimpleTextArea::create(jsonutil::stringOr(item,"description"),"chatFont.fnt",.43f,216);desc->setAnchorPoint({.5f,1});desc->setAlignment(kCCTextAlignmentCenter);desc->setMaxLines(2);desc->setPosition({x,124});m_view->addChild(desc);
  bool owned=jsonutil::boolOr(item,"owned"),equipped=kind=="arrow"?owned&&arrow::skinFromItem(jsonutil::stringOr(item,"id"))==arrow::ArrowSettings::skin():jsonutil::boolOr(item,"equipped");auto price=jsonutil::integerOr(item,"price");
  auto acquisition=jsonutil::stringOr(item,"acquisition");bool exclusive=acquisition=="crate"||acquisition=="pass";
  std::string text=exclusive&&!owned?(acquisition=="pass"?"Recompensa do GeoPass":"Exclusivo de caixas"):equipped?"Usando":owned?"Equipar":price==0?"Gratis":fmt::format("Comprar: {} Tokens",price);
  auto b=ui::makeTextButton(text,216,equipped?"GJ_button_04.png":"GJ_button_01.png",this,menu_selector(TierShopPopup::onItem),.31f,30);b->setTag(index);b->setPosition({x,77});b->setEnabled(!m_busy&&!equipped&&(!exclusive||owned));menu->addChild(b);
 }
 label(m_view,fmt::format("{} / {}",m_page+1,pages),{275,39},.3f,94,{181,215,239});
 for(int side=0;side<2;++side){auto b=ui::makeTextButton(side?">":"<",53,"GJ_button_04.png",this,menu_selector(TierShopPopup::onPage),.34f,25);b->setTag(side?1:-1);b->setPosition({side?347.f:203.f,39});b->setEnabled(!m_busy&&(side?m_page+1<pages:m_page>0));b->setVisible(pages>1);menu->addChild(b);}
}
void TierShopPopup::onItem(CCObject* sender){
 int i=sender->getTag();if(m_busy||i<0||i>=static_cast<int>(m_items.size()))return;auto item=m_items[i];
 if(isPendingShopOrder(pending())){retryOrder();return;}
 if(jsonutil::boolOr(item,"owned")&&jsonutil::stringOr(item,"kind")=="arrow"){
  arrow::ArrowSettings::equip(arrow::skinFromItem(jsonutil::stringOr(item,"id")));m_status->setString("Arrow equipado!");render();return;
 }
 if(jsonutil::boolOr(item,"owned")){
  m_busy=true;render();m_request.rpc("geobr_equip_item",matjson::makeObject({{"p_item_id",jsonutil::stringOr(item,"id")}}),[this](auto r){bool ok=r.success;finish(std::move(r));if(ok)m_status->setString("Item equipado no seu perfil!");});return;
 }
 auto price=jsonutil::integerOr(item,"price"),balance=jsonutil::integerOr(m_state,"tokens");
 if(price>balance){createQuickPopup("Tokens insuficientes",fmt::format("Voce tem <cy>{}</c> Tokens. Faltam <cy>{}</c> para este item.\nComplete Quest Pass e resgate as recompensas do GeoPass.",balance,price-balance),"OK",nullptr,[](auto,bool){});return;}
 WeakRef<TierShopPopup> weak=this;
 createQuickPopup("Confirmar compra",fmt::format("<cy>{}</c>\nPreco: <cy>{} Tokens</c>\nSaldo depois: <cg>{} Tokens</c>\n{}",jsonutil::stringOr(item,"name"),price,balance-price,jsonutil::stringOr(item,"description")),"Cancelar","Comprar",[weak,item](auto,bool yes){auto self=weak.lock();if(yes&&self&&!self->m_busy)self->buy(item);});
}
void TierShopPopup::buy(matjson::Value const& item){
 auto args=shopArguments(item,m_state,visual::uuid());
 if(!savePending(matjson::makeObject({{"owner",CloudService::userID()},{"args",args}}))){m_status->setString("Nao foi possivel salvar a compra.");return;}
 retryOrder();
}
void TierShopPopup::retryOrder(){
 if(m_busy)return;auto p=pending();if(!isPendingShopOrder(p)){refresh();return;}
 if(jsonutil::stringOr(p,"owner")!=CloudService::userID()){m_status->setString("Compra pendente pertence a outra sessao.");return;}
 m_busy=true;m_status->setString("Conferindo compra...");render();
 auto args=p.get("args").unwrap();
 // Recover old journal entries whose optional values were written as {}.
 for(auto key:{"p_expected_run","p_expected_tier"}){auto value=args.get(key);if(value&&value.unwrap().isObject())args[key]=nullptr;}
 m_request.rpc("geobr_buy_item",args,[this](auto r){
  bool rejected=r.httpStatus>=400&&r.httpStatus<500&&r.httpStatus!=401&&r.httpStatus!=429;
  bool ok=r.success;
  if(ok||rejected)savePending(matjson::Value(nullptr));
  if(!ok&&!rejected)r.error="Compra pendente. Toque Atualizar para conferir.";
  finish(std::move(r));
  if(ok){m_status->setString("Compra concluida!");Notification::create("Compra concluida!",NotificationIcon::Success)->show();}
 });
}
}
