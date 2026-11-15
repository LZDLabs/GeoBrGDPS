-- Directory and ownership regression. All fixtures roll back.
begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
 ('7a020fac-16f8-4f88-a784-01f09b7b0181','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-v18-a@example.invalid',now(),now(),true),
 ('7a020fac-16f8-4f88-a784-01f09b7b0182','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-v18-b@example.invalid',now(),now(),true);
select set_config('request.jwt.claim.sub','7a020fac-16f8-4f88-a784-01f09b7b0181',true);
select set_config('request.jwt.claims','{"sub":"7a020fac-16f8-4f88-a784-01f09b7b0181","role":"authenticated","is_anonymous":true}',true);
set local role authenticated;
do $$declare s jsonb;failed boolean=false;begin
 s=public.geobr_profile_sync('V18 Fixture Alpha',12,255,65280);
 assert s->'profile'->>'display_name'='V18 Fixture Alpha';
 assert (s->'profile'->>'icon_id')::int=12 and (s->'profile'->>'lifetime_xp')::bigint=0;
 assert (public.geobr_players('fixture ALPHA',0)->>'total')::int=1;
 assert jsonb_array_length(public.geobr_players('fixture ALPHA',3)->'players')=0;
 assert (public.geobr_players('%',0)->>'total')::int=0,'wildcard is literal';
 begin perform public.geobr_profile_sync('Invalid',-1,255,65280);exception when others then failed=true;end;
 assert failed,'avatar range validated';
 failed=false;
 begin update public.geobr_profiles set lifetime_xp=99999 where user_id=auth.uid();exception when insufficient_privilege then failed=true;end;
 assert failed,'client cannot write permanent XP';
end $$;
reset role;
select set_config('request.jwt.claim.sub','7a020fac-16f8-4f88-a784-01f09b7b0182',true);
select set_config('request.jwt.claims','{"sub":"7a020fac-16f8-4f88-a784-01f09b7b0182","role":"authenticated","is_anonymous":true}',true);
set local role authenticated;
do $$declare s jsonb;row jsonb;begin
 perform public.geobr_profile_sync('V18 Fixture Beta',3,65535,16711680);
 s=public.geobr_players('V18 Fixture',0);
 assert (s->>'total')::int=2 and jsonb_array_length(s->'players')=2;
 row=s->'players'->0;
 assert row->>'display_name'='V18 Fixture Alpha','other mod profile visible';
 assert row->>'title_name'='Iniciante' and row->>'theme_slug'='default';
 assert not(row ? 'tokens') and not(row ? 'inventory') and not(row ? 'email');
 assert not exists(select 1 from public.geobr_inventory where user_id='7a020fac-16f8-4f88-a784-01f09b7b0181');
end $$;
reset role;
select 'PASS: directory lookup, other player profile, pagination, literal search, avatar validation and protected XP' as result;
rollback;
