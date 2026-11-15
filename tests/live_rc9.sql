-- GeoBR RC9 live checks: full player icons, groups + chat, collab rooms
-- (create/join by code/pull/push with conflicts/chat/publish/credits/leave).
-- Run after the migration, inside one transaction. ROLLS BACK: nothing persists.
begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
('9cf657ba-740a-4903-9c85-0000000000a1','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc9-a@example.invalid',now(),now(),true),
('9cf657ba-740a-4903-9c85-0000000000b2','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc9-b@example.invalid',now(),now(),true),
('9cf657ba-740a-4903-9c85-0000000000c3','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc9-c@example.invalid',now(),now(),true);
create temporary table rc9(k text primary key, v text) on commit drop;
grant all on rc9 to authenticated;

-- Player A: full icon sync.
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000a1',true);
set local role authenticated;
do $$declare s jsonb;denied boolean;begin
 perform public.geobr_state_v2();
 s=public.geobr_profile_sync_v3('RC9 Alfa',12,16711680,255,'',true,65280,'{"ship":3,"ball":7,"ufo":2,"wave":5,"robot":1,"spider":4,"swing":2,"jetpack":1}'::jsonb);
 denied=false;begin perform public.geobr_profile_sync_v3('RC9 Alfa',12,1,2,'',true,65280,'{"car":3}'::jsonb);exception when others then denied=true;end;assert denied,'unknown icon kind refused';
 denied=false;begin perform public.geobr_profile_sync_v3('RC9 Alfa',12,1,2,'',true,99999999,'{}'::jsonb);exception when others then denied=true;end;assert denied,'glow colour range';
 denied=false;begin perform public.geobr_profile_sync_v3('RC9 Alfa',12,1,2,'',true,1,'{"ship":2.5}'::jsonb);exception when others then denied=true;end;assert denied,'icon ids are integers';
 s=public.geobr_players('RC9 Alfa',0);
 assert s->'players'->0->>'glow'='true' and (s->'players'->0->>'glow_color')::int=65280 and (s->'players'->0->'icons'->>'ship')::int=3,'players list carries glow and icons';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000b2',true);
set local role authenticated;
do $$begin perform public.geobr_state_v2();perform public.geobr_profile_sync_v3('RC9 Beta',3,255,65280,'',false,16777215,'{}'::jsonb);end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000c3',true);
set local role authenticated;
do $$begin perform public.geobr_state_v2();perform public.geobr_profile_sync_v3('RC9 Gama',4,255,65280,'',false,16777215,'{}'::jsonb);end $$;
reset role;

-- Groups: A creates, adds B; B chats; C is outside; ownership passes on leave.
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000a1',true);
set local role authenticated;
do $$declare g jsonb;denied boolean;begin
 denied=false;begin perform public.geobr_group_create('ab');exception when others then denied=true;end;assert denied,'group name length';
 g=public.geobr_group_create('Time RC9');
 insert into rc9 values('group',g->>'id');
 perform public.geobr_group_add((g->>'id')::uuid,'9cf657ba-740a-4903-9c85-0000000000b2');
 perform public.geobr_group_add((g->>'id')::uuid,'9cf657ba-740a-4903-9c85-0000000000b2');
 assert jsonb_array_length(public.geobr_group_members((g->>'id')::uuid))=2,'two members, add is idempotent';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000b2',true);
set local role authenticated;
do $$declare g uuid=(select v from rc9 where k='group')::uuid;id uuid=gen_random_uuid();denied boolean;begin
 assert jsonb_array_length(public.geobr_group_list())=1,'B sees the group';
 perform public.geobr_group_send(g,'oi grupo',id);
 perform public.geobr_group_send(g,'oi grupo',id);
 assert jsonb_array_length(public.geobr_group_messages(g))=1,'send is idempotent by id';
 denied=false;begin perform public.geobr_group_send(g,'de novo',gen_random_uuid());exception when others then denied=true;end;assert denied,'3 second rate limit';
 denied=false;begin perform public.geobr_group_remove(g,'9cf657ba-740a-4903-9c85-0000000000a1');exception when others then denied=true;end;assert denied,'only the owner removes';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000c3',true);
set local role authenticated;
do $$declare g uuid=(select v from rc9 where k='group')::uuid;denied boolean;begin
 denied=false;begin perform public.geobr_group_messages(g);exception when others then denied=true;end;assert denied,'outsider cannot read the chat';
 denied=false;begin perform public.geobr_group_add(g,'9cf657ba-740a-4903-9c85-0000000000c3');exception when others then denied=true;end;assert denied,'outsider cannot add';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000a1',true);
set local role authenticated;
do $$declare g uuid=(select v from rc9 where k='group')::uuid;begin
 perform public.geobr_group_leave(g);
end $$;
reset role;
do $$begin
 assert (select owner from geobr_private.groups where id=(select v from rc9 where k='group')::uuid)='9cf657ba-740a-4903-9c85-0000000000b2','ownership passed to B';
 assert exists(select 1 from public.geobr_social_notifications where owner='9cf657ba-740a-4903-9c85-0000000000b2' and kind='group'),'B was notified';
end $$;

-- Collab: A creates with a level, B joins by code, both push, conflict, chat, publish, credits.
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000a1',true);
set local role authenticated;
do $$declare r jsonb;begin
 r=public.geobr_collab_create('Sala RC9','H4sIBASE');
 assert r->>'code' ~ '^[A-HJ-NP-Z2-9]{8}$','8-char unambiguous code';
 assert (r->>'revision')::int=1 and r->>'role'='owner','owner at revision 1';
 insert into rc9 values('room',r->>'id'),('code',r->>'code');
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000c3',true);
set local role authenticated;
do $$declare r jsonb;denied boolean;i int;begin
 denied=false;begin perform public.geobr_collab_pull((select v from rc9 where k='room')::uuid);exception when others then denied=true;end;assert denied,'non-member cannot pull';
 for i in 1..10 loop r=public.geobr_collab_join('ZZZZZZZZ'); assert not (r->>'ok')::boolean,'wrong code'; end loop;
 r=public.geobr_collab_join((select v from rc9 where k='code'));
 assert not (r->>'ok')::boolean and r->>'error' like 'Muitas tentativas%','brute force blocked after 10 failures';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000b2',true);
set local role authenticated;
do $$declare r jsonb;room uuid=(select v from rc9 where k='room')::uuid;begin
 r=public.geobr_collab_join(lower((select v from rc9 where k='code')));
 assert (r->>'ok')::boolean and r->'room'->>'role'='editor','B joined as editor (code is case-insensitive)';
 r=public.geobr_collab_pull(room);
 assert r->>'level'='H4sIBASE' and (r->>'revision')::int=1,'pull returns the level';
 r=public.geobr_collab_push(room,'H4sIBOB',1);
 assert (r->>'ok')::boolean and (r->>'revision')::int=2,'push on the current revision';
 perform public.geobr_collab_send(room,'mexi no comeco',gen_random_uuid());
 assert jsonb_array_length(public.geobr_collab_rooms())=1,'B lists the room';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000a1',true);
set local role authenticated;
do $$declare r jsonb;room uuid=(select v from rc9 where k='room')::uuid;denied boolean;begin
 r=public.geobr_collab_push(room,'H4sIALICE',1);
 assert not (r->>'ok')::boolean and (r->>'conflict')::boolean and r->>'level'='H4sIBOB' and (r->>'revision')::int=2,'stale push returns the newer level';
 r=public.geobr_collab_push(room,'H4sIMERGED',2);
 assert (r->>'ok')::boolean and (r->>'revision')::int=3,'merged push accepted';
 assert jsonb_array_length(public.geobr_collab_messages(room))=1,'A reads the room chat';
 assert jsonb_array_length(public.geobr_collab_members(room))=2,'two members';
 r=public.geobr_collab_publish(room,987654321);
 assert (r->>'ok')::boolean,'publish';
 assert public.geobr_collab_published() @> '[987654321]'::jsonb,'published ids';
 r=public.geobr_collab_credits(987654321);
 assert (r->>'found')::boolean and jsonb_array_length(r->'members')=2 and r->>'title'='Sala RC9','credits list';
 assert not (r ? 'code'),'credits never reveal the code';
 denied=false;begin perform public.geobr_collab_push(room,repeat('x',8388609),3);exception when others then denied=true;end;assert denied,'8 MiB limit';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000c3',true);
set local role authenticated;
do $$declare r jsonb;begin
 r=public.geobr_collab_credits(987654321);
 assert (r->>'found')::boolean,'anyone with a session sees credits';
 assert not (public.geobr_collab_credits(1)->>'found')::boolean,'unknown level';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000a1',true);
set local role authenticated;
do $$begin perform public.geobr_collab_leave((select v from rc9 where k='room')::uuid);end $$;
reset role;
do $$begin
 assert (select owner_id from public.geobr_collab_rooms where id=(select v from rc9 where k='room')::uuid)='9cf657ba-740a-4903-9c85-0000000000b2','room passed to B';
 assert not has_function_privilege('anon','public.geobr_collab_pull(uuid)','execute'),'anon cannot call collab';
 assert not has_function_privilege('authenticated','geobr_private.player_json(uuid)','execute'),'player_json is internal';
 assert exists(select 1 from geobr_private.releases where version='1.9.0-beta.9' and enabled),'RC9 release enabled';
 assert exists(select 1 from public.geobr_main_levels where id=3 and gmd_path='main-levels/geobr-bloqueio-total-v1.gmd'),'RC8 campaign rows';
 assert not exists(select 1 from pg_policies where policyname like 'geobr_tmp_seed%'),'temporary storage policies removed';
end $$;
select 'PASS live_rc9: icons+glow sync, groups (create/add/chat/rate limit/outsiders/ownership), collab (code, brute force, pull/push/conflict, chat, publish, credits, leave), grants, RC8 rows' as result;
rollback;
