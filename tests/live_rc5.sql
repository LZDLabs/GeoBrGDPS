begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
('9cf657ba-740a-4903-9c85-000000000051','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc5-test@example.invalid',now(),now(),true),
('9cf657ba-740a-4903-9c85-000000000052','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc5-peer@example.invalid',now(),now(),true);
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000051',true);
select public.geobr_state_v2();
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000052',true);
select public.geobr_state_v2();
do $$declare denied boolean=false;begin
 begin perform public.geobr_moderation('players');exception when others then denied=true;end;assert denied,'non-admin denied';
end $$;
insert into geobr_private.admin_sessions(user_id,expires_at) values(auth.uid(),now()+interval '20 minutes');
do $$declare r jsonb;begin
 r=public.geobr_moderation('punish','{"user_id":"9cf657ba-740a-4903-9c85-000000000051","kind":"mute","minutes":60,"reason":"SQL test only"}');assert (r->>'ok')::boolean;
 assert jsonb_array_length(public.geobr_moderation('versions'))>=2;
end $$;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000051',true);
set local role authenticated;
do $$declare s jsonb;denied boolean=false;begin
 s=public.geobr_access_status();assert not(s->>'chat_allowed')::boolean and not(s->>'dm_allowed')::boolean and(s->>'allowed')::boolean;
 begin perform public.geobr_dm_send('9cf657ba-740a-4903-9c85-000000000052','test',gen_random_uuid());exception when others then denied=true;end;assert denied,'mute prevents DM';
 denied=false;begin insert into public.geobr_community_messages(username,message,user_id) values('test','test',auth.uid());exception when others then denied=true;end;assert denied,'mute prevents direct REST post';
 s=public.geobr_state_v2();assert s ? 'profile','mute does not remove progression';
end $$;
reset role;
update geobr_private.restrictions set expires_at=now()-interval '1 second' where user_id=auth.uid();
do $$begin assert(public.geobr_access_status()->>'chat_allowed')::boolean,'automatic expiry';end $$;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000052',true);
select public.geobr_moderation('punish','{"user_id":"9cf657ba-740a-4903-9c85-000000000051","kind":"chat","minutes":0,"reason":"chat test"}');
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000051',true);
set local role authenticated;
do $$declare r jsonb;begin
 assert not(public.geobr_access_status()->>'chat_allowed')::boolean;
 assert(public.geobr_access_status()->>'dm_allowed')::boolean;
 perform public.geobr_dm_send('9cf657ba-740a-4903-9c85-000000000052','allowed DM',gen_random_uuid());
 r=public.geobr_dm_thread('9cf657ba-740a-4903-9c85-000000000052');assert jsonb_array_length(r)=1;
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000052',true);
do $$declare r jsonb;begin r=public.geobr_dm_inbox();assert (r->0->>'unread')::int=1,'new DM unread';perform public.geobr_dm_thread('9cf657ba-740a-4903-9c85-000000000051');r=public.geobr_dm_inbox();assert(r->0->>'unread')::int=0,'thread marks seen';end $$;
select public.geobr_moderation('punish','{"user_id":"9cf657ba-740a-4903-9c85-000000000051","kind":"mod","minutes":60,"reason":"mod test"}');
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000051',true);
set local role authenticated;
do $$declare denied boolean=false;begin
 assert not(public.geobr_access_status()->>'allowed')::boolean;
 begin perform public.geobr_state_v2();exception when others then denied=true;end;assert denied,'mod ban prevents economy';
 assert(select count(*) from public.geobr_dm_messages)=0,'mod ban prevents private message read';
end $$;
reset role;
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000052',true);
do $$declare i bigint;begin for i in select id from geobr_private.restrictions where kind='mod' and user_id='9cf657ba-740a-4903-9c85-000000000051' loop perform public.geobr_moderation('revoke',jsonb_build_object('id',i));end loop;end $$;
select public.geobr_moderation('save_version','{"version":"0.0.0-disabled","enabled":false,"message":"Upgrade test"}');
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000051',true);
select set_config('request.headers','{"x-geobr-version":"0.0.0-disabled"}',true);
set local role authenticated;
do $$declare denied boolean=false;begin
 assert not(public.geobr_access_status()->>'allowed')::boolean;
 begin perform public.geobr_state_v2();exception when others then denied=true;end;assert denied,'disabled version denied';
end $$;
reset role;
select set_config('request.headers','{"x-geobr-version":"1.9.0-beta.5"}',true);
do $$begin assert(public.geobr_access_status()->>'allowed')::boolean;assert (select count(*) from geobr_private.moderation_log where actor='9cf657ba-740a-4903-9c85-000000000052')=5,'audit trail';end $$;
rollback;
