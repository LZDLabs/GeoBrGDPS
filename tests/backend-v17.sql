-- Integration checks against the real RPC/RLS contract. All fixtures roll back.
begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
 ('7a020fac-16f8-4f88-a784-01f09b7b0171','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-v17-test-a@example.invalid',now(),now(),true),
 ('7a020fac-16f8-4f88-a784-01f09b7b0172','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-v17-test-b@example.invalid',now(),now(),true);
select set_config('request.jwt.claim.sub','7a020fac-16f8-4f88-a784-01f09b7b0171',true);
select set_config('request.jwt.claims','{"sub":"7a020fac-16f8-4f88-a784-01f09b7b0171","role":"authenticated","is_anonymous":true}',true);
select public.geobr_state_v2();
update public.geobr_wallets set tokens=1000 where user_id='7a020fac-16f8-4f88-a784-01f09b7b0171';
set local role authenticated;
do $$
declare s jsonb; previous jsonb; e jsonb; q jsonb; run uuid; first_run uuid; req uuid; failed boolean; i int; j int; n int; expected_xp bigint=0; coins bigint; xp bigint; sid bigint; levelkey text; origin text;
begin
 s=public.geobr_state_v2();sid=(s->'season'->>'id')::bigint;first_run=(s->'quest'->>'run_id')::uuid;
 assert (s->>'quest_count')::int=64 and jsonb_array_length(s->'shop')=18 and jsonb_array_length(s->'rewards')=30,'seed catalog';
 assert not (s ? 'quests') and (s->'profile'->>'level')::int=1,'one active quest and permanent level';
 -- A Main Level's server metadata overrides the difficulty reported by a client.
 e=jsonb_build_object('id',gen_random_uuid(),'attempt',gen_random_uuid(),'run',first_run,'kind','complete','scope','main','level','main:1','difficulty','auto','mode','normal','platformer','classic','percent',100);
 s=public.geobr_activity(e);assert (s->'quest'->>'progress')::int=0,'normal Main cannot be forged as Auto';
 for i in 1..64 loop
  q=s->'quest';run=(q->>'run_id')::uuid;assert (q->>'sequence_order')::int=i,'correct sequential order';
  origin=case when q->>'scope'='any' then 'online' else q->>'scope' end;
  if i=2 then
   e=e||jsonb_build_object('id',gen_random_uuid(),'attempt',gen_random_uuid(),'run',run,'kind','death','scope','online','level','online:555','percent',45);
   s=public.geobr_activity(e);assert (s->'quest'->>'progress')::int=0,'45 is not 46';
  end if;
  for j in 1..(q->>'goal')::int loop
   levelkey=origin||':'||case when origin='main' then '1' else (100+j)::text end;
   e=jsonb_build_object('id',gen_random_uuid(),'attempt',gen_random_uuid(),'run',run,'kind',q->>'kind','scope',origin,'level',levelkey,
    'difficulty',case when q->>'difficulty'='any' then 'unrated' else q->>'difficulty' end,
    'mode',case when q->>'mode'='any' then 'normal' else q->>'mode' end,
    'platformer',case when q->>'platformer'='any' then 'classic' else q->>'platformer' end,
    'percent',case when q->>'kind'='complete' then 100 else (q->>'min_percent')::int end);
   s=public.geobr_activity(e);previous=s;s=public.geobr_activity(e);
   assert s->'quest'=previous->'quest' and s->>'xp'=previous->>'xp','replayed event never advances';
   if i=1 and j=1 then
    s=public.geobr_activity(e||jsonb_build_object('id',gen_random_uuid(),'attempt',gen_random_uuid()));
    assert (s->'quest'->>'progress')::int=1,'two Autos must be distinct levels';
   end if;
  end loop;
  expected_xp=expected_xp+(q->>'xp')::bigint;
  assert (s->>'xp')::bigint=expected_xp and (s->'profile'->>'lifetime_xp')::bigint=expected_xp,'XP exactly once per quest';
 end loop;
 assert (s->'quest'->>'sequence_order')::int=1 and (s->'quest'->>'completed_total')::int=64 and (s->'quest'->>'run_id')::uuid<>first_run,'sequence wraps into new run';
 -- Purchases, retries, ownership and skipping a question.
 coins=(s->>'tokens')::bigint;req=gen_random_uuid();
 s=public.geobr_buy_item('theme-fire',req,60);assert (s->>'tokens')::bigint=coins-60,'correct debit';
 s=public.geobr_buy_item('theme-fire',req,60);assert (s->>'tokens')::bigint=coins-60,'retry not charged';
 s=public.geobr_buy_item('theme-fire',gen_random_uuid(),60);assert (s->>'tokens')::bigint=coins-60,'owned not charged';
 s=public.geobr_equip_item('theme-fire');assert s->'profile'->>'theme_slug'='fire','owned theme equips';
 failed=false;begin perform public.geobr_equip_item('theme-ice');exception when others then failed=true;end;assert failed,'cannot equip unowned';
 failed=false;begin perform public.geobr_buy_item('theme-ice',gen_random_uuid(),1);exception when others then failed=true;end;assert failed,'server price';
 run=(s->'quest'->>'run_id')::uuid;coins=(s->>'tokens')::bigint;xp=(s->>'xp')::bigint;req=gen_random_uuid();
 s=public.geobr_buy_item('skip-quest',req,30,run,null);assert (s->>'tokens')::bigint=coins-30 and (s->>'xp')::bigint=xp and (s->'profile'->>'lifetime_xp')::bigint=xp,'skip has no quest XP or token reward';
 previous=s;s=public.geobr_buy_item('skip-quest',req,30,run,null);assert s->'quest'=previous->'quest','skip retry exactly once';
 failed=false;begin perform public.geobr_buy_item('skip-quest',gen_random_uuid(),30,run,null);exception when others then failed=true;end;assert failed,'stale skip rejected';
 update public.geobr_shop_items set price=0 where id='theme-gold';get diagnostics n=row_count;assert n=0,'catalog RLS';
 update public.geobr_quest_steps set xp=9999 where season_id=sid;get diagnostics n=row_count;assert n=0,'quest RLS';
 failed=false;begin update public.geobr_wallets set tokens=999999;exception when insufficient_privilege then failed=true;end;assert failed,'no wallet writes';
 failed=false;begin update public.geobr_profiles set lifetime_xp=999999;exception when insufficient_privilege then failed=true;end;assert failed,'no direct XP writes';
 failed=false;begin perform public.geobr_pass_claim_quest(1);exception when others then failed=true;end;assert failed,'old parallel quests retired';
end $$;
reset role;
-- Tier skip at a partial tier, independently from permanent player XP.
update public.geobr_pass_progress set xp=225 where user_id='7a020fac-16f8-4f88-a784-01f09b7b0171';
set local role authenticated;
do $$declare s jsonb; p jsonb; req uuid=gen_random_uuid(); failed boolean=false;begin
 p=public.geobr_state_v2();s=public.geobr_buy_item('skip-tier',req,80,null,2);
 assert (s->>'xp')::int=300 and s->'profile'->>'lifetime_xp'=p->'profile'->>'lifetime_xp' and s->'quest'=p->'quest','tier skip boundaries';
 p=s;s=public.geobr_buy_item('skip-tier',req,80,null,2);assert s->>'tokens'=p->>'tokens' and s->>'xp'=p->>'xp','tier retry';
 begin perform public.geobr_buy_item('skip-tier',gen_random_uuid(),80,null,2);exception when others then failed=true;end;assert failed,'stale tier rejected';
end $$;
reset role;
update public.geobr_wallets set tokens=0 where user_id='7a020fac-16f8-4f88-a784-01f09b7b0171';
set local role authenticated;
do $$declare failed boolean=false;begin
 begin perform public.geobr_buy_item('theme-ice',gen_random_uuid(),60);exception when others then failed=true;end;
 assert failed and not exists(select 1 from public.geobr_inventory where item_id='theme-ice'),'insufficient funds atomic';
end $$;
select set_config('request.jwt.claim.sub','7a020fac-16f8-4f88-a784-01f09b7b0172',true);
select set_config('request.jwt.claims','{"sub":"7a020fac-16f8-4f88-a784-01f09b7b0172","role":"authenticated","is_anonymous":true}',true);
do $$declare s jsonb;begin s=public.geobr_state_v2();assert (s->>'xp')::int=0 and (s->>'tokens')::int=0 and not exists(select 1 from public.geobr_inventory),'accounts isolated';end $$;
reset role;
-- A future draft receives editable copies of the catalog, not players' progression.
do $$declare sid bigint;begin
 insert into public.geobr_pass_seasons(name,starts_at,ends_at,published) values('TEST ROLLBACK',now()+interval '90 days',now()+interval '120 days',false) returning id into sid;
 assert (select count(*) from public.geobr_quest_steps where season_id=sid)=64,'new season quest templates';
 assert (select count(*) from public.geobr_pass_rewards where season_id=sid)=30,'new season reward templates';
end $$;
rollback;
select 'PASS: all 64 quests, sequence wrap, exact 46%, distinct levels, server metadata, XP, atomic purchases, skips, RLS, new season templates' as result;
