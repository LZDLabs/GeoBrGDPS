-- GeoBR RC5. Private moderation records, auditable decisions, reversible release controls.
create table if not exists geobr_private.restrictions (
 id bigint generated always as identity primary key,
 user_id uuid not null references auth.users(id), kind text not null check(kind in ('mute','chat','mod')),
 reason text not null check(length(reason) between 3 and 200), expires_at timestamptz,
 created_at timestamptz not null default now(), created_by uuid not null references auth.users(id),
 revoked_at timestamptz, revoked_by uuid references auth.users(id)
);
create index if not exists restrictions_active on geobr_private.restrictions(user_id,kind) where revoked_at is null;
alter table geobr_private.restrictions enable row level security;
revoke all on geobr_private.restrictions from public,anon,authenticated;
create table if not exists geobr_private.releases (
 version text primary key check(length(version) between 1 and 40), enabled boolean not null default true,
 message text not null default 'Atualize o GeoBR pelo canal oficial.', updated_at timestamptz not null default now()
);
alter table geobr_private.releases enable row level security;
revoke all on geobr_private.releases from public,anon,authenticated;
insert into geobr_private.releases(version) values('1.9.0-beta.4'),('1.9.0-beta.5') on conflict do nothing;
create table if not exists geobr_private.moderation_log (
 id bigint generated always as identity primary key, actor uuid not null, action text not null,
 details jsonb not null, created_at timestamptz not null default now()
);
alter table geobr_private.moderation_log enable row level security;
revoke all on geobr_private.moderation_log from public,anon,authenticated;
create or replace function geobr_private.access_status(p_version text default null) returns jsonb
language plpgsql stable security definer set search_path='' as $$
declare u uuid=auth.uid(); v_version text; headers jsonb; r geobr_private.restrictions; release geobr_private.releases; result jsonb;
begin
 headers=coalesce(nullif(current_setting('request.headers',true),'')::jsonb,'{}');
 v_version=coalesce(nullif(p_version,''),headers->>'x-geobr-version','1.9.0-beta.4');
 result=jsonb_build_object('allowed',true,'chat_allowed',true,'dm_allowed',true,'version',v_version);
 select * into release from geobr_private.releases where releases.version=v_version;
 if found and not release.enabled then result=result||jsonb_build_object('allowed',false,'reason',release.message,'code','version_disabled');end if;
 if u is not null then
  select * into r from geobr_private.restrictions where user_id=u and kind='mod' and revoked_at is null and (expires_at is null or expires_at>now()) order by id desc limit 1;
  if found then result=result||jsonb_build_object('allowed',false,'chat_allowed',false,'dm_allowed',false,'reason',r.reason,'until',r.expires_at,'code','mod_banned');end if;
  select * into r from geobr_private.restrictions where user_id=u and kind in ('mute','chat') and revoked_at is null and (expires_at is null or expires_at>now()) order by (kind='mute') desc,id desc limit 1;
  if found then result=result||jsonb_build_object('chat_allowed',false,'dm_allowed',r.kind<>'mute','chat_reason',r.reason,'chat_until',r.expires_at,'chat_code',r.kind);end if;
 end if;
 return result;
end $$;
revoke all on function geobr_private.access_status(text) from public,anon;
grant execute on function geobr_private.access_status(text) to authenticated;
create or replace function public.geobr_access_status(p_version text default null) returns jsonb
language sql stable security invoker set search_path='' as $$select geobr_private.access_status(p_version)$$;
revoke all on function public.geobr_access_status(text) from public,anon;
grant execute on function public.geobr_access_status(text) to authenticated;
create or replace function geobr_private.require_access(p_scope text default 'mod') returns void
language plpgsql stable security definer set search_path='' as $$
declare s jsonb;begin
 if auth.uid() is null then raise exception 'Sessao necessaria';end if;
 s=geobr_private.access_status();
 if not (s->>'allowed')::boolean then raise exception '%',s->>'reason';end if;
 if p_scope='chat' and not (s->>'chat_allowed')::boolean then raise exception 'Chat bloqueado: % (ate %)',s->>'chat_reason',coalesce(s->>'chat_until','revisao do ADM');end if;
 if p_scope='dm' and not (s->>'dm_allowed')::boolean then raise exception 'Envio suspenso: %',s->>'chat_reason';end if;
end $$;
revoke all on function geobr_private.require_access(text) from public,anon,authenticated;
-- Gate all economy state paths; mutations in the same transaction roll back when denied.
alter function geobr_private.state_v2() rename to state_rc4;
create function geobr_private.state_v2() returns jsonb language plpgsql security definer set search_path='' as $$
begin perform geobr_private.require_access();return geobr_private.state_rc4();end $$;
revoke all on function geobr_private.state_v2() from public,anon;
grant execute on function geobr_private.state_v2() to authenticated;
create or replace function geobr_private.social_gate() returns trigger language plpgsql security definer set search_path='' as $$
begin perform geobr_private.require_access(case when TG_TABLE_NAME='geobr_dm_messages' then 'dm' else 'chat' end);return new;end $$;
revoke all on function geobr_private.social_gate() from public,anon,authenticated;
create trigger geobr_moderation_gate before insert on public.geobr_community_messages for each row execute function geobr_private.social_gate();
create trigger geobr_dm_moderation_gate before insert on public.geobr_dm_messages for each row execute function geobr_private.social_gate();
create or replace function geobr_private.moderation(p_action text,p_data jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare u uuid=auth.uid();target uuid;kind text;mins int;reason text;r jsonb;v_release text;
begin
 if not geobr_private.is_admin() then raise exception 'Sessao ADM necessaria';end if;
 if p_action='players' then
  return (select coalesce(jsonb_agg(to_jsonb(t)),'[]') from (select user_id,display_name from public.geobr_profiles order by display_name limit 300)t);
 elsif p_action='list' then
  return jsonb_build_object('restrictions',(select coalesce(jsonb_agg(to_jsonb(t)),'[]') from (select rest.*,p.display_name from geobr_private.restrictions rest left join public.geobr_profiles p using(user_id) order by rest.id desc limit 100)t),'log',(select coalesce(jsonb_agg(to_jsonb(t)),'[]') from (select * from geobr_private.moderation_log order by id desc limit 50)t));
 elsif p_action='versions' then return (select coalesce(jsonb_agg(to_jsonb(rel) order by rel.version),'[]') from geobr_private.releases rel);
 elsif p_action='punish' then
  target=(p_data->>'user_id')::uuid;kind=p_data->>'kind';mins=(p_data->>'minutes')::int;reason=btrim(p_data->>'reason');
  if target=u or target is null or not exists(select 1 from public.geobr_profiles where user_id=target) then raise exception 'Jogador invalido';end if;
  if kind is null or kind not in ('mute','chat','mod') or mins is null or mins<0 or mins>525600 or (kind='mute' and mins=0) or reason is null or length(reason) not between 3 and 200 then raise exception 'Informe tipo, prazo valido e motivo (3 a 200 caracteres)';end if;
  insert into geobr_private.restrictions(user_id,kind,reason,expires_at,created_by) values(target,kind,reason,case when mins=0 then null else now()+mins*interval '1 minute' end,u) returning to_jsonb(restrictions) into r;
 elsif p_action='revoke' then
  update geobr_private.restrictions set revoked_at=now(),revoked_by=u where id=(p_data->>'id')::bigint and revoked_at is null returning to_jsonb(restrictions) into r;
  if r is null then raise exception 'Restricao nao encontrada ou ja revogada';end if;
 elsif p_action='save_version' then
  v_release=btrim(p_data->>'version');reason=btrim(p_data->>'message');
  if v_release is null or v_release !~ '^[0-9][0-9A-Za-z.+-]{0,39}$' or reason is null or length(reason) not between 3 and 200 or jsonb_typeof(p_data->'enabled') is distinct from 'boolean' then raise exception 'Versao ou mensagem invalida';end if;
  insert into geobr_private.releases(version,enabled,message) values(v_release,(p_data->>'enabled')::boolean,reason) on conflict on constraint releases_pkey do update set enabled=excluded.enabled,message=excluded.message,updated_at=now() returning to_jsonb(releases) into r;
 elsif p_action='remove_message' then
  delete from public.geobr_community_messages where id=(p_data->>'id')::bigint returning jsonb_build_object('id',id,'user_id',user_id,'message',message) into r;
  if r is null then raise exception 'Mensagem nao encontrada';end if;
 else raise exception 'Acao invalida';end if;
 insert into geobr_private.moderation_log(actor,action,details) values(u,p_action,r);
 return jsonb_build_object('ok',true,'result',r);
end $$;
revoke all on function geobr_private.moderation(text,jsonb) from public,anon;
grant execute on function geobr_private.moderation(text,jsonb) to authenticated;
create or replace function public.geobr_moderation(p_action text,p_data jsonb default '{}') returns jsonb
language sql security invoker set search_path='' as $$select geobr_private.moderation(p_action,p_data)$$;
revoke all on function public.geobr_moderation(text,jsonb) from public,anon;
grant execute on function public.geobr_moderation(text,jsonb) to authenticated;
