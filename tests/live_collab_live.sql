-- GeoBR 1.12.0 live collab checks. One transaction, ROLLED BACK: nothing persists.
begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
('9cf657ba-740a-4903-9c85-0000000000d1','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-live-a@example.invalid',now(),now(),true),
('9cf657ba-740a-4903-9c85-0000000000e2','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-live-b@example.invalid',now(),now(),true);
create temporary table lv(k text primary key, v text) on commit drop;
grant all on lv to authenticated;
-- Host A: room, seed, first live edit.
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000d1',true);
set local role authenticated;
do $$declare r jsonb;begin
 perform public.geobr_state_v2(); perform public.geobr_profile_sync_v3('Live Host',1,1,2,'',false,1,'{}'::jsonb);
 r=public.geobr_collab_create('Sala Ao Vivo','');
 assert (r->>'can_save')::boolean,'host can save';
 insert into lv values('room',r->>'id'),('code',r->>'code');
 r=public.geobr_collab_live_seed((r->>'id')::uuid,'[{"u":"s0","d":"1,1,2,15,3,15"},{"u":"s1","d":"1,8,2,45,3,15"}]'::jsonb);
 assert (r->>'ok')::boolean and (r->>'seq')::int=1,'seed';
 r=public.geobr_collab_live_seed((select v from lv where k='room')::uuid,'[{"u":"x","d":"1,1"}]'::jsonb);
 assert not (r->>'ok')::boolean,'second seed refused';
 r=public.geobr_collab_live((select v from lv where k='room')::uuid,1,'[{"u":"a.1","d":"1,211,2,105,3,15"}]'::jsonb,100,200);
 assert (r->>'seq')::int=2 and jsonb_array_length(r->'ops')=1 and (r->'ops'->0->>'mine')::boolean and jsonb_array_length(r->'presence')=0,'host op';
end $$;
reset role;
-- Member B: joins, loads, sees host, edits and deletes; cannot save.
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000e2',true);
set local role authenticated;
do $$declare r jsonb;denied boolean;room uuid=(select v from lv where k='room')::uuid;begin
 perform public.geobr_state_v2(); perform public.geobr_profile_sync_v3('Live Membro',1,1,2,'',false,1,'{}'::jsonb);
 r=public.geobr_collab_join((select v from lv where k='code'));
 assert (r->>'ok')::boolean and not (r->'room'->>'can_save')::boolean,'member joins without save';
 r=public.geobr_collab_live_load(room);
 assert (r->>'seq')::int=2 and jsonb_array_length(r->'objects')=3,'load has the 3 objects';
 r=public.geobr_collab_live(room,2,'[{"u":"s1","d":"1,8,2,60,3,15"},{"u":"s0","d":null}]'::jsonb,300,50);
 assert (r->>'seq')::int=4 and r->'presence'->0->>'name'='Live Host' and (r->'presence'->0->>'x')::real=100,'member sees host position';
 denied=false;begin perform public.geobr_collab_push(room,'H4sIX',-1);exception when others then denied=true;end;assert denied,'member cannot save';
end $$;
reset role;
-- Host A: receives B's edits, marks B as admin.
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000d1',true);
set local role authenticated;
do $$declare r jsonb;room uuid=(select v from lv where k='room')::uuid;begin
 r=public.geobr_collab_live(room,2,'[]'::jsonb,100,200);
 assert jsonb_array_length(r->'ops')=2 and r->'ops'->1->>'d' is null and not (r->'ops'->0->>'mine')::boolean,'host gets edit + delete';
 assert r->'presence'->0->>'name'='Live Membro','host sees member';
 assert (public.geobr_collab_push(room,'H4sIX',-1)->>'ok')::boolean,'host saves';
 r=public.geobr_collab_set_saver(room,'9cf657ba-740a-4903-9c85-0000000000e2',true);
 assert (select (m->>'can_save')::boolean from jsonb_array_elements(public.geobr_collab_members(room)) m where m->>'user_id'='9cf657ba-740a-4903-9c85-0000000000e2'),'member is admin';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-0000000000e2',true);
set local role authenticated;
do $$begin assert (public.geobr_collab_push((select v from lv where k='room')::uuid,'H4sIY',-1)->>'ok')::boolean,'admin saves'; end $$;
reset role;
select 'PASS live_collab_live: seed, live ops both ways, delete, presence, save only host/admin' as result;
rollback;
