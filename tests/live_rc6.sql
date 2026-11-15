-- GeoBR RC6 live checks. Runs inside a transaction and ROLLS BACK: no
-- fixture, message or profile change survives.
begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
('9cf657ba-740a-4903-9c85-000000000061','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc6-a@example.invalid',now(),now(),true),
('9cf657ba-740a-4903-9c85-000000000062','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc6-b@example.invalid',now(),now(),true);

select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000061',true);
set local role authenticated;
select public.geobr_profile_sync_v2('Tester RC6',1,4382975,10573311,'');

-- Chat identity is bound to the profile, not to the client-sent username.
insert into public.geobr_community_messages(username,message,user_id) values('ADM GeoBR','rc6 identity test',auth.uid());
do $$declare n text;begin
 select username into n from public.geobr_community_messages where user_id=auth.uid() order by id desc limit 1;
 assert n='Tester RC6', format('chat username must come from the profile, got %s', n);
end $$;

-- Arrow cosmetics: valid values persist, invalid ones are rejected.
do $$declare r jsonb;denied boolean=false;s smallint;g boolean;begin
 r=public.geobr_profile_arrow(3,true);assert (r->>'ok')::boolean;
 select arrow_skin,arrow_glow into s,g from public.geobr_profiles where user_id=auth.uid();
 assert s=3 and g, 'arrow skin/glow stored';
 begin perform public.geobr_profile_arrow(7,false);exception when others then denied=true;end;
 assert denied,'skin outside 1..4 rejected';
end $$;
reset role;

-- Remote profiles expose the Arrow skin through geobr_native_profile.
insert into public.geobr_account_links(account_id,user_id) values(987654321,'9cf657ba-740a-4903-9c85-000000000061');
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000062',true);
set local role authenticated;
select public.geobr_state_v2();
do $$declare p jsonb;begin
 p=public.geobr_native_profile(987654321);
 assert (p->>'arrow_skin')::int=3 and (p->>'arrow_glow')::boolean, 'native profile carries arrow fields';
 assert not (p ? 'user_id'), 'native profile still hides user_id';
end $$;
reset role;

do $$begin
 assert not has_function_privilege('anon','public.geobr_profile_arrow(integer,boolean)','execute'),'anon cannot set arrow';
 assert has_function_privilege('authenticated','public.geobr_profile_arrow(integer,boolean)','execute'),'players can set arrow';
 assert exists(select 1 from geobr_private.releases where version='1.9.0-beta.6' and enabled),'RC6 release enabled';
 assert exists(select 1 from geobr_private.releases where version='1.9.0-beta.5' and enabled),'RC5 still enabled during rollout';
end $$;

select 'PASS live_rc6: chat identity bound to profile, arrow skin/glow RPC + validation, native profile exposure, grants, release rows' as result;
rollback;
