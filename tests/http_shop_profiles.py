#!/usr/bin/env python3
"""Actual Data API check using a disposable anonymous test session.
Pass --auth-file outside the source tree and --body from shop_order_test.
Provision only this test user's wallet with 250 Tokens; delete the test user afterwards.
Never run against a real player's auth file.
"""
import argparse,json,pathlib,urllib.request,urllib.error,uuid
p=argparse.ArgumentParser();p.add_argument('--auth-file',type=pathlib.Path,required=True);p.add_argument('--body',type=pathlib.Path,required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];m=json.loads((root/'mod.json').read_text());url=m['settings']['supabase-url']['default'];key=m['settings']['supabase-publishable-key']['default']
auth=json.loads(a.auth_file.read_text());uid=auth['user']['id'];token=auth['access_token']
def call(path,body=None,method='POST',expected=200):
 data=None if body is None else json.dumps(body).encode()
 req=urllib.request.Request(url+path,data,headers={'apikey':key,'Authorization':'Bearer '+token,'Content-Type':'application/json','Prefer':'return=representation','User-Agent':'GeoBR-Mod/1.8-test'},method=method)
 try:
  with urllib.request.urlopen(req,timeout=25) as r:status=r.status;raw=r.read()
 except urllib.error.HTTPError as e:status=e.code;raw=e.read()
 if expected is not None:assert status==expected,(path,status,raw[:350])
 return status,json.loads(raw) if raw else None
def rpc(name,body={}):return call('/rest/v1/rpc/'+name,body)[1]
def purchase(item,price,state):
 return {'p_item_id':item,'p_request_id':str(uuid.uuid4()),'p_expected_price':price,'p_expected_run':state['quest']['run_id'] if state.get('quest') else None,'p_expected_tier':state['xp']//state['season']['xp_per_tier'] if state.get('season') else None}
s=rpc('geobr_state_v2');assert s['tokens']==250 and s['profile']['display_name']=='GeoBR QA 18','Only a freshly seeded disposable fixture may run this test'
# Exact serialization produced by production ShopOrder.hpp, including JSON null.
args=json.loads(a.body.read_text());assert args['p_expected_run'] is None and args['p_expected_tier'] is None
s=rpc('geobr_buy_item',args);assert s['tokens']==190 and any(i['id']=='theme-fire' and i['owned'] for i in s['shop'])
s=rpc('geobr_buy_item',args);assert s['tokens']==190
s=rpc('geobr_equip_item',{'p_item_id':'theme-fire'});assert s['profile']['theme_slug']=='fire'
print('PASS HTTP: comprar tema, repetir pedido sem novo debito e equipar',flush=True)
s=rpc('geobr_buy_item',purchase('title-antigao',40,s));assert s['tokens']==150
s=rpc('geobr_equip_item',{'p_item_id':'title-antigao'});assert s['profile']['title_name']=='Antigao'
q=s['quest']['run_id'];xp=s['xp'];personal=s['profile']['lifetime_xp']
skip=purchase('skip-quest',30,s);s=rpc('geobr_buy_item',skip);assert s['tokens']==120 and s['quest']['run_id']!=q and s['xp']==xp
s=rpc('geobr_buy_item',skip);assert s['tokens']==120
skip=purchase('skip-tier',80,s);s=rpc('geobr_buy_item',skip);assert s['tokens']==40 and s['xp']==100 and s['profile']['lifetime_xp']==personal
print('PASS HTTP: titulo, skip de quest e skip de etapa sem inflar nivel pessoal',flush=True)
status,error=call('/rest/v1/rpc/geobr_buy_item',purchase('theme-gold',150,s),expected=400);assert 'Tokens insuficientes' in error['message']
assert rpc('geobr_state_v2')['tokens']==40
status,_=call('/rest/v1/geobr_wallets?user_id=eq.'+uid,{'tokens':9999},method='PATCH',expected=None);assert status in (401,403)
s=rpc('geobr_profile_sync',{'p_name':'GeoBR QA 18','p_icon':12,'p_color1':4382975,'p_color2':10573311});assert s['profile']['icon_id']==12 and s['profile']['theme_slug']=='fire'
players=rpc('geobr_players',{'p_query':'qa 18','p_offset':0});assert players['total']==1 and players['players'][0]['user_id']==uid
for row in players['players']:assert not {'tokens','inventory','email','access_token'} & row.keys()
assert rpc('geobr_players',{'p_query':'%','p_offset':0})['total']==0
_,public=call('/rest/v1/geobr_profiles?user_id=eq.'+uid+'&select=display_name,theme_slug,title_name,lifetime_xp,icon_id',method='GET');assert public[0]['theme_slug']=='fire' and public[0]['title_name']=='Antigao'
print('PASS HTTP: saldo insuficiente, RLS da carteira, perfil, busca e arte equipada publica',flush=True)
call('/auth/v1/logout?scope=global',{},expected=204)
print('Test session signed out. Remove only its auth.users row to clean up.',flush=True)
