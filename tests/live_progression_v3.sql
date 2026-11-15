begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
('9cf657ba-740a-4903-9c85-000000000011','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-v3-test@example.invalid',now(),now(),true),
('9cf657ba-740a-4903-9c85-000000000012','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-v3-peer@example.invalid',now(),now(),true);
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000011',true);
set local role authenticated;
select public.geobr_state_v2();
reset role;
update public.geobr_profiles set lifetime_xp=900 where user_id='9cf657ba-740a-4903-9c85-000000000011';
insert into public.geobr_crates(user_id,rarity,source) values('9cf657ba-740a-4903-9c85-000000000011',6,'test-crate');
set local role authenticated;
do $$declare s jsonb;r jsonb;oldtokens bigint;cid uuid;denied boolean=false;begin
 s=public.geobr_state_v2();assert (s->'profile'->>'level')::int=4;assert jsonb_array_length(s->'level_rewards')=6;
 r=public.geobr_progression('level','{"level":2}');assert (r->'state'->>'claimed_level')::int=2;oldtokens=(r->'state'->>'tokens')::bigint;
 r=public.geobr_progression('level','{"level":2}');assert (r->'state'->>'tokens')::bigint=oldtokens,'level replay idempotent';
 r=public.geobr_progression('level','{"level":3}');assert r->'reward'->>'kind'='orbs';
 begin perform public.geobr_progression('level','{"level":5}');exception when others then denied=true;end;assert denied,'locked level denied';
 select id into cid from public.geobr_crates where source='test-crate';
 r=public.geobr_progression('open',jsonb_build_object('id',cid));assert r->'reward' is not null;
 s=public.geobr_progression('open',jsonb_build_object('id',cid));assert r->'reward'=s->'reward','crate replay same loot';
 assert jsonb_array_length(s->'state'->'crates')=0;
 denied=false;begin perform public.geobr_save_rank(123456,9876,true);exception when others then denied=true;end;assert denied,'rank RPC admin only';
 denied=false;begin perform public.geobr_season_builder('generate','{}');exception when others then denied=true;end;assert denied,'builder admin only';
 perform public.geobr_progression('drop',jsonb_build_object('id',gen_random_uuid(),'level','online:123456','kind','complete','mode','normal','percent',100,'elapsed',90,'difficulty','demon'));
 perform public.geobr_progression('drop',jsonb_build_object('id',gen_random_uuid(),'level','online:123456','kind','complete','mode','normal','percent',100,'elapsed',90,'difficulty','demon'));
end $$;
reset role;
do $$begin assert (select count(*) from geobr_private.drop_rolls where user_id='9cf657ba-740a-4903-9c85-000000000011')=1,'same level one roll/day';end $$;
insert into geobr_private.admin_sessions(user_id,expires_at) values('9cf657ba-740a-4903-9c85-000000000011',now()+interval '10 minutes');
set local role authenticated;
do $$declare r jsonb;s jsonb;n bigint;begin
 r=public.geobr_save_rank(123456,9876,true);assert (r->>'position')::int=9876;
 r=public.geobr_save_rank_v3((r->>'id')::bigint,123457,9876,true);assert (r->>'level_id')::bigint=123457,'editing TOP replaces the level';
 assert not exists(select 1 from public.geobr_level_ranks where level_id=123456);
 r=public.geobr_season_builder('generate','{"name":"V3 test","days":7,"tiers":10,"quests":8,"xp_per_tier":100}');
 assert jsonb_array_length(r->'config'->'rewards')=10;assert jsonb_array_length(r->'config'->'quests')=8;
 s=public.geobr_season_builder('publish',jsonb_build_object('id',r->>'id'));n=(s->>'season_id')::bigint;
 assert (select count(*) from public.geobr_pass_rewards where season_id=n)=10;
 assert (select count(*) from public.geobr_quest_steps where season_id=n)=8;
 s=public.geobr_season_builder('publish',jsonb_build_object('id',r->>'id'));assert (s->>'season_id')::bigint=n,'publish retry idempotent';
end $$;
reset role;
do $$declare r jsonb;s jsonb;v_reward bigint;run text;denied boolean=false;begin
 -- Validate native skin bounds for every draw without granting fabricated items.
 for i in 1..1000 loop r=geobr_private.roll_skin();assert r->>'icon_type' in ('cube','ship','ball','ufo','wave','robot','spider','swing');assert (r->>'icon_id')::int between 10 and 99;end loop;
 update geobr_private.progression set skips=2 where user_id=auth.uid();
 s=public.geobr_state_v2();run=s->'quest'->>'run_id';
 r=public.geobr_progression('skip',jsonb_build_object('run',run));assert (r->'state'->>'skips')::int=1;
 begin perform public.geobr_progression('skip',jsonb_build_object('run',run));exception when others then denied=true;end;assert denied,'stale skip rejected';
 assert (public.geobr_state_v2()->>'skips')::int=1;
 insert into public.geobr_pass_progress(user_id,season_id,xp) values(auth.uid(),(s->'season'->>'id')::bigint,500) on conflict(user_id,season_id) do update set xp=500;
 select id into v_reward from public.geobr_pass_rewards where season_id=(s->'season'->>'id')::bigint and tier=5 and bonus_crate>0;
 assert v_reward is not null;
 perform public.geobr_reward_v2(v_reward);perform public.geobr_reward_v2(v_reward);
 assert (select count(*) from public.geobr_crates where user_id=auth.uid() and source='bonus:'||v_reward)=1,'pass bonus is idempotent';
end $$;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000012',true);
set local role authenticated;
do $$begin assert (select count(*) from public.geobr_crates)=0,'other users cannot see crates';end $$;
reset role;
rollback;
