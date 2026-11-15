-- Run in a privileged SQL session on a test database with the initial seed.
-- Set geobr.test_password privately before running; the actual password is not shipped.
begin;
insert into auth.users(id,aud,role,is_anonymous,created_at,updated_at)
values('df8150cd-4db3-46c3-92ad-0df10a6dff06','authenticated','authenticated',true,now(),now());
select set_config('request.jwt.claim.sub','df8150cd-4db3-46c3-92ad-0df10a6dff06',true);
set local role authenticated;
do $$
declare result jsonb; blocked boolean; rid bigint; qid bigint; sid bigint; lid bigint; nid bigint;
begin
 if geobr_private.is_admin() then raise exception 'TEST: ordinary player is admin'; end if;
 blocked=false;
 begin insert into public.geobr_news(title,description) values('QA','must not publish'); exception when insufficient_privilege then blocked=true; end;
 if not blocked then raise exception 'TEST: nonadmin news insert allowed'; end if;
 blocked=false;
 begin perform public.geobr_pass_claim_reward((select min(id) from public.geobr_pass_rewards)); exception when others then blocked=true; end;
 if not blocked then raise exception 'TEST: reward without XP allowed'; end if;
 select id into sid from public.geobr_pass_seasons where published and starts_at<=now() and ends_at>now();
 select min(id) into lid from public.geobr_main_levels where published;
 select id into qid from public.geobr_pass_quests where season_id=sid and event='main_normal' and goal=1 limit 1;
 perform public.geobr_pass_complete(lid,false);
 perform public.geobr_pass_complete(lid,false);
 result=public.geobr_pass_claim_quest(qid);
 if (result->>'xp')::int<>100 then raise exception 'TEST: XP should be 100: %',result; end if;
 result=public.geobr_pass_claim_quest(qid);
 if (result->>'xp')::int<>100 then raise exception 'TEST: duplicate XP'; end if;
 select id into rid from public.geobr_pass_rewards where season_id=sid and tier=1;
 result=public.geobr_pass_claim_reward(rid);
 result=public.geobr_pass_claim_reward(rid);
 if (result->>'tokens')::int<>25 then raise exception 'TEST: tokens should be credited once'; end if;
 if (select count(*) from public.geobr_reward_claims)<>1 then raise exception 'TEST: duplicate reward or cross-user exposure'; end if;
 result=public.geobr_admin_login('wrong-password');
 if (result->>'ok')::boolean then raise exception 'TEST: wrong password allowed'; end if;
 result=public.geobr_admin_login(current_setting('geobr.test_password'));
 if not (result->>'ok')::boolean or not geobr_private.is_admin() then raise exception 'TEST: correct password denied'; end if;
 insert into public.geobr_news(title,description,published) values('QA rollback','QA rollback',false) returning id into nid;
 update public.geobr_news set title='QA edited' where id=nid;
 if not exists(select 1 from public.geobr_news where id=nid and title='QA edited') then raise exception 'TEST: admin edit failed'; end if;
 delete from public.geobr_news where id=nid;
 perform public.geobr_admin_logout();
 if geobr_private.is_admin() then raise exception 'TEST: logout failed'; end if;
end $$;
reset role;
rollback;
