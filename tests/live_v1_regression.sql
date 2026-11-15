begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
 ('9cf657ba-740a-4903-9c85-000000000001','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-v1-a@example.invalid',now(),now(),true),
 ('9cf657ba-740a-4903-9c85-000000000002','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-v1-b@example.invalid',now(),now(),true),
 ('9cf657ba-740a-4903-9c85-000000000003','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-v1-c@example.invalid',now(),now(),true);
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000002',true);
set local role authenticated;
select public.geobr_profile_sync_v2('V1 Test B',10,65535,16711935,'test-pack:cube');
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000001',true);
set local role authenticated;
do $$declare s jsonb;n jsonb;event jsonb;reward bigint;beforexp integer;begin
 s=public.geobr_profile_sync_v2('V1 Test A',12,65535,16711935,'');
 assert s->'season'->>'name'='GeoBR - Expedicao';
 assert s->'quest'->>'kind'='complete';
 event=jsonb_build_object('id',gen_random_uuid(),'attempt',gen_random_uuid(),'run',s->'quest'->>'run_id','kind','complete','scope','online','level','online:987654321','difficulty','easy','mode','normal','platformer','classic','percent',100);
 n=public.geobr_activity(event);assert (n->>'xp')::int=100,'first quest gives 100XP';assert (n->>'tokens')::int=10,'quest tokens';
 assert (public.geobr_activity(event)->>'tokens')::int=10,'event replay does not repeat reward';
 select id into reward from public.geobr_pass_rewards where season_id=(s->'season'->>'id')::bigint and tier=1;
 n=public.geobr_reward_v2(reward);assert (n->>'tokens')::int=40,'pass reward delivered';
 n=public.geobr_reward_v2(reward);assert (n->>'tokens')::int=40,'claim replay is idempotent';
 perform public.geobr_dm_send('9cf657ba-740a-4903-9c85-000000000002','Hello','9cf657ba-740a-4903-9c85-000000000100',null);
 perform public.geobr_dm_send('9cf657ba-740a-4903-9c85-000000000002','Hello','9cf657ba-740a-4903-9c85-000000000100',null);
 assert (select count(*) from public.geobr_dm_messages where id='9cf657ba-740a-4903-9c85-000000000100')=1;
 assert exists(select 1 from jsonb_array_elements(public.geobr_players('V1 Test B',0)->'players') p where p->>'icon_slug'='test-pack:cube');
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000003',true);
set local role authenticated;
do $$declare denied boolean=false;begin
 assert (select count(*) from public.geobr_dm_messages)=0,'third user cannot read DMs';
 assert jsonb_array_length(public.geobr_dm_thread('9cf657ba-740a-4903-9c85-000000000002'))=0;
 begin insert into public.geobr_level_ranks(level_id,position) values(987654321,999);exception when insufficient_privilege then denied=true;end;
 assert denied,'players cannot edit ranking';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000002',true);
set local role authenticated;
do $$begin
 assert (select count(*) from public.geobr_dm_messages)=1;
 assert (select count(*) from public.geobr_social_notifications)=1;
 perform public.geobr_dm_block('9cf657ba-740a-4903-9c85-000000000001');
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000001',true);
set local role authenticated;
do $$declare denied boolean=false;begin
 begin perform public.geobr_dm_send('9cf657ba-740a-4903-9c85-000000000002','Blocked','9cf657ba-740a-4903-9c85-000000000101',null);exception when others then denied=true;end;
 assert denied,'blocked sends rejected';
end $$;
rollback;
