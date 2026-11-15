begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
('9cf657ba-740a-4903-9c85-000000000041','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc4-test@example.invalid',now(),now(),true),
('9cf657ba-740a-4903-9c85-000000000042','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc4-peer@example.invalid',now(),now(),true);
insert into public.geobr_crates(user_id,rarity,source) select '9cf657ba-740a-4903-9c85-000000000041',n,'rc4-'||n from generate_series(1,6) n;
insert into public.geobr_crates(user_id,rarity,source) values('9cf657ba-740a-4903-9c85-000000000041',1,'legacy');
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000041',true);
set local role authenticated;
do $$declare c record;r jsonb;s jsonb;n integer;beforetokens bigint;denied boolean=false;e jsonb;begin
 s=public.geobr_state_v2();
 e=jsonb_build_object('id',gen_random_uuid(),'attempt',gen_random_uuid(),'run',s->'quest'->>'run_id','kind','complete','scope','online','level','online:987654321','difficulty','easy','mode','normal','platformer','classic','percent',100,'elapsed',60);
 r=public.geobr_activity_v4(e);assert (r->'state'->>'xp')::int=100,'quest confirmed in the event response';
 r=public.geobr_activity_v4(e);assert (r->'state'->>'xp')::int=100,'event retry does not grant XP twice';
 for c in select id,rarity from public.geobr_crates where source like 'rc4-%' order by rarity loop
  n=(array[4,5,6,7,8,10])[c.rarity];
  r=public.geobr_open_crate_v4(c.id);assert jsonb_array_length(r->'items')=n,'correct slot count';
  beforetokens=(r->'state'->>'tokens')::bigint;
  s=public.geobr_open_crate_v4(c.id);assert s->'items'=r->'items','retry returns identical bundle';assert (s->'state'->>'tokens')::bigint=beforetokens,'retry does not duplicate wallet';
  assert r->'items'->0->>'kind'='tokens';assert r->'items'->1->>'kind'='orbs';
 end loop;
 r=public.geobr_progression('open',jsonb_build_object('id',(select id from public.geobr_crates where source='legacy')));
 beforetokens=(r->'state'->>'tokens')::bigint;
 s=public.geobr_open_crate_v4((select id from public.geobr_crates where source='legacy'));
 assert jsonb_array_length(s->'items')=1 and s->'reward'=r->'reward','old openings stay unchanged';
 assert (s->'state'->>'tokens')::bigint=beforetokens,'old opening not granted again';
 begin perform public.geobr_buy_item('crate-theme-aurora',gen_random_uuid(),0,null,null);exception when others then denied=true;end;assert denied,'exclusive cannot be bought for free';
end $$;
reset role;
do $$declare r jsonb;s jsonb;begin
 assert (select count(*) from public.geobr_reward_claims where user_id=auth.uid() and source_key like 'crate:%:slot:%')=40,'all 40 slots stored independently';
 r=geobr_private.deliver('rc4:exclusive-title','{"kind":"title","amount":1,"item_id":"crate-title-hunter"}');
 assert r->>'kind'='title' or (r->>'duplicate')::boolean;
 r=geobr_private.deliver('rc4:exclusive-theme','{"kind":"theme","amount":1,"item_id":"crate-theme-aurora"}');
 assert r->>'kind'='theme' or (r->>'duplicate')::boolean;
 s=public.geobr_equip_item('crate-title-hunter');assert s->'profile'->>'title_name'='Cacador de Tesouros';
 s=public.geobr_equip_item('crate-theme-aurora');assert s->'profile'->>'theme_slug'='aurora';
 r=geobr_private.deliver('rc4:duplicate','{"kind":"theme","amount":1,"item_id":"crate-theme-aurora"}');assert r->>'kind'='tokens' and (r->>'amount')::int=100;
end $$;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000042',true);
set local role authenticated;
do $$declare denied boolean=false;begin
 assert (select count(*) from public.geobr_crates)=0;
 begin perform public.geobr_open_crate_v4('9cf657ba-740a-4903-9c85-000000000041');exception when others then denied=true;end;assert denied,'foreign crate denied';
end $$;
reset role;
rollback;
