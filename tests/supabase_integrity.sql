-- Rollback-only checks: own wallet, sender identity, chat cooldown, season overlap, missing files.
begin;
insert into auth.users(id,aud,role,is_anonymous,created_at,updated_at)
values('491be08a-b5b8-43fd-aacd-3d2bbf1cfc2e','authenticated','authenticated',true,now(),now());
insert into public.geobr_wallets(user_id,tokens) values('491be08a-b5b8-43fd-aacd-3d2bbf1cfc2e',99);
select set_config('request.jwt.claim.sub','491be08a-b5b8-43fd-aacd-3d2bbf1cfc2e',true);
set local role authenticated;
do $$
begin
 if (select count(*) from public.geobr_wallets)<>1 then raise exception 'Wallet RLS failed'; end if;
 begin
  insert into public.geobr_community_messages(user_id,username,message) values('00000000-0000-0000-0000-000000000001','QA','Not sent');
  raise exception 'Spoof was accepted';
 exception when insufficient_privilege then null; when raise_exception then
  if sqlerrm='Spoof was accepted' then raise; end if;
 end;
 insert into public.geobr_community_messages(username,message) values('QA','Temporary rollback-only message');
 begin
  insert into public.geobr_community_messages(username,message) values('QA','Must be limited');
  raise exception 'Rate limit missing';
 exception when raise_exception then
  if sqlerrm='Rate limit missing' then raise; end if;
 end;
end $$;
reset role;
do $$
begin
 begin
  insert into public.geobr_pass_seasons(name,starts_at,ends_at,published) values('Overlapping QA',now(),now()+interval '1 day',true);
  raise exception 'Overlap accepted';
 exception when exclusion_violation then null;
 end;
 begin
  insert into public.geobr_main_levels(campaign_order,name,creator,difficulty,stars,gmd_path,published)
  values(98765,'Missing file QA','QA','Normal',3,'main-levels/qa-file-does-not-exist.gmd',true);
  raise exception 'Missing file published';
 exception when raise_exception then
  if sqlerrm='Missing file published' then raise; end if;
 end;
end $$;
rollback;
