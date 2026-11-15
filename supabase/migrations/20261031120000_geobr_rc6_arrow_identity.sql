-- GeoBR RC6 (1.9.0-beta.6) — forward-only migration. Do not re-run older
-- migrations; this file only adds/replaces the objects listed below.

-- 1) Chat identity: RC5 trusted the client-sent username, so any player could
--    post as "ADM". The trigger now copies the sender's profile name.
create or replace function geobr_private.validate_message()
 returns trigger
 language plpgsql
 security definer
 set search_path to ''
as $function$
declare v_name text;
begin
 if auth.uid() is null or new.user_id is distinct from auth.uid() then raise exception 'Sessao invalida'; end if;
 if length(btrim(new.message))<1 or length(new.message)>160 then raise exception 'Mensagem invalida'; end if;
 select display_name into v_name from public.geobr_profiles where user_id=auth.uid();
 new.username=left(coalesce(nullif(btrim(v_name),''),nullif(btrim(new.username),''),'Jogador GeoBR'),40);
 perform pg_advisory_xact_lock(hashtextextended(auth.uid()::text,0));
 if exists(select 1 from public.geobr_community_messages where user_id=auth.uid() and created_at>now()-interval '3 seconds') then raise exception 'Aguarde 3 segundos antes de enviar novamente'; end if;
 new.created_at=now();return new;
end $function$;

-- 2) Arrow cosmetics on the public profile (read through the existing
--    "Cosmetic profiles public" policy and geobr_native_profile).
alter table public.geobr_profiles
  add column if not exists arrow_skin smallint not null default 1,
  add column if not exists arrow_glow boolean not null default false;
do $$ begin
  if not exists (select 1 from pg_constraint where conname='geobr_profiles_arrow_skin_check') then
    alter table public.geobr_profiles add constraint geobr_profiles_arrow_skin_check check (arrow_skin between 1 and 4);
  end if;
end $$;

create or replace function geobr_private.profile_arrow(p_skin integer, p_glow boolean)
 returns jsonb
 language plpgsql
 security definer
 set search_path to ''
as $function$
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 perform geobr_private.require_access('mod');
 if p_skin is null or p_skin not between 1 and 4 or p_glow is null then raise exception 'Skin invalida'; end if;
 perform geobr_private.ensure_v2();
 update public.geobr_profiles set arrow_skin=p_skin,arrow_glow=p_glow,updated_at=now()
  where user_id=auth.uid() and (arrow_skin,arrow_glow) is distinct from (p_skin::smallint,p_glow);
 return jsonb_build_object('ok',true,'arrow_skin',p_skin,'arrow_glow',p_glow);
end $function$;
revoke all on function geobr_private.profile_arrow(integer, boolean) from public, anon;
grant execute on function geobr_private.profile_arrow(integer, boolean) to authenticated;

create or replace function public.geobr_profile_arrow(p_skin integer, p_glow boolean)
 returns jsonb
 language sql
 security invoker
 set search_path to ''
as $function$ select geobr_private.profile_arrow(p_skin, p_glow) $function$;
revoke all on function public.geobr_profile_arrow(integer, boolean) from public, anon;
grant execute on function public.geobr_profile_arrow(integer, boolean) to authenticated;

-- 3) RC6 release row (enabled). beta.5 stays enabled for the rollout.
insert into geobr_private.releases(version, enabled, message)
values ('1.9.0-beta.6', true, 'GeoBR RC6')
on conflict (version) do nothing;

-- 4) Covering indexes for the foreign keys flagged by the performance advisor.
create index if not exists geobr_dm_reads_peer_idx on geobr_private.dm_reads(peer);
create index if not exists geobr_restrictions_created_by_idx on geobr_private.restrictions(created_by);
create index if not exists geobr_restrictions_revoked_by_idx on geobr_private.restrictions(revoked_by);
create index if not exists geobr_season_drafts_owner_idx on geobr_private.season_drafts(owner);
create index if not exists geobr_season_drafts_published_idx on geobr_private.season_drafts(published_season);
create index if not exists geobr_community_reply_idx on public.geobr_community_messages(reply_to);
create index if not exists geobr_dm_blocks_peer_idx on public.geobr_dm_blocks(peer);
create index if not exists geobr_dm_reply_idx on public.geobr_dm_messages(reply_to);
create index if not exists geobr_pass_rewards_item_idx on public.geobr_pass_rewards(item_id);
create index if not exists geobr_social_actor_idx on public.geobr_social_notifications(actor);
