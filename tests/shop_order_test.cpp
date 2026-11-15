#include "../src/models/ShopOrder.hpp"
#include <cassert>
#include <iostream>
using namespace geobr;
int main(){
 // This was the Beta 1.7 blocker: a default matjson value is an OBJECT.
 matjson::Value legacyIdle;
 assert(legacyIdle.isObject());
 assert(!isPendingShopOrder(legacyIdle));
 assert(!isPendingShopOrder(matjson::Value(nullptr)));
 auto item=matjson::makeObject({{"id","theme-fire"},{"price",60}});
 auto args=shopArguments(item,matjson::Value::object(),"7a020fac-16f8-4f88-a784-01f09b7b0181");
 assert(args["p_expected_run"].isNull()&&args["p_expected_tier"].isNull());
 auto order=matjson::makeObject({{"owner","user-a"},{"args",args}});
 assert(isPendingShopOrder(order));
 auto disk=matjson::parse(order.dump()).unwrap();
 assert(isPendingShopOrder(disk));
 assert(disk["args"]["p_request_id"]==args["p_request_id"]);
 assert(!isPendingShopOrder(matjson::makeObject({{"owner","user-a"},{"args",matjson::Value::object()}})));
 auto state=matjson::makeObject({{"xp",625},{"season",matjson::makeObject({{"xp_per_tier",100}})},{"quest",matjson::makeObject({{"run_id","7a020fac-16f8-4f88-a784-01f09b7b0182"}})}});
 auto skip=shopArguments(matjson::makeObject({{"id","skip-tier"},{"price",80}}),state,"7a020fac-16f8-4f88-a784-01f09b7b0183");
 assert(skip["p_expected_tier"].asInt().unwrap()==6);
 assert(skip["p_expected_run"].asString().unwrap()=="7a020fac-16f8-4f88-a784-01f09b7b0182");
 auto cleared=matjson::parse("null").unwrap();assert(!isPendingShopOrder(cleared));
 // JSON below is sent unchanged by the HTTP integration check.
 std::cout<<args.dump()<<"\n";
 std::cerr<<"PASS: legacy empty journal, nullable RPC arguments, persisted request identity, skip snapshot and cleared purchase\n";
}
