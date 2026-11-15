-- GeoBR RC9 (1.9.0-beta.9) — collab editor, player groups, full player icons.
-- Forward-only. Idempotent where possible. Every table added here lives in
-- geobr_private (not exposed by the API) or is reached only through
-- SECURITY DEFINER functions with explicit membership checks.

-- 0) RC8 content (campaign + beta.8), in case 20261105180000 was not applied.
drop policy if exists geobr_tmp_seed_rc8 on storage.objects;
drop policy if exists geobr_tmp_seed_rc8b_insert on storage.objects;
drop policy if exists geobr_tmp_seed_rc8b_select on storage.objects;
drop policy if exists geobr_tmp_seed_rc8b_delete on storage.objects;
insert into public.geobr_main_levels
  (id, campaign_order, name, creator, description, difficulty, stars, song, platformer, gmd_path, file_version, published, coins)
values
  (2, 2, 'Teoria Tropical', 'Equipe GeoBR',
   'Fase original na musica Theory of Everything: cubo, nave, UFO, bola e dois trechos de Arrow.',
   'Hard', 5, 'Theory of Everything - DJ-Nate', false, 'main-levels/geobr-teoria-tropical-v1.gmd', 1, true, 0),
  (3, 3, 'Bloqueio Total', 'Equipe GeoBR',
   'Fase original na musica Deadlocked: tres trechos de Arrow e velocidades de 0.5x a 2x.',
   'Harder', 7, 'Deadlocked - F-777', false, 'main-levels/geobr-bloqueio-total-v1.gmd', 1, true, 0)
on conflict (id) do nothing;
insert into geobr_private.releases(version, enabled, message) values ('1.9.0-beta.8', true, 'GeoBR RC8') on conflict (version) do nothing;

-- 1) Notifications for groups and collabs.
alter table public.geobr_social_notifications drop constraint if exists geobr_social_notifications_kind_check;
alter table public.geobr_social_notifications add constraint geobr_social_notifications_kind_check
  check (kind = any (array['dm','reply','group','collab']));

-- 2) Profiles carry the whole icon set (glow + every gamemode).
alter table public.geobr_profiles
  add column if not exists glow boolean not null default false,
  add column if not exists glow_color integer not null default 16777215,
  add column if not exists icons jsonb not null default '{}'::jsonb;
alter table public.geobr_profiles drop constraint if exists geobr_profiles_glow_color_check;
alter table public.geobr_profiles add constraint geobr_profiles_glow_color_check check (glow_color between 0 and 16777215);
alter table public.geobr_profiles drop constraint if exists geobr_profiles_icons_check;
alter table public.geobr_profiles add constraint geobr_profiles_icons_check check (jsonb_typeof(icons) = 'object' and octet_length(icons::text) <= 512);

create or replace function geobr_private.profile_sync_v3(p_name text, p_icon integer, p_color1 integer, p_color2 integer,
  p_icon_slug text, p_glow boolean, p_glow_color integer, p_icons jsonb)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare k text; v jsonb; clean jsonb = '{}'::jsonb; n numeric;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 if p_glow_color is null or p_glow_color not between 0 and 16777215 then raise exception 'Cor do glow invalida'; end if;
 if p_icons is null or jsonb_typeof(p_icons) <> 'object' then raise exception 'Icones invalidos'; end if;
 for k, v in select * from jsonb_each(p_icons) loop
  if k not in ('ship','ball','ufo','wave','robot','spider','swing','jetpack') or jsonb_typeof(v) <> 'number' then raise exception 'Icones invalidos'; end if;
  n = (v #>> '{}')::numeric;
  if n <> trunc(n) or n not between 1 and 2000 then raise exception 'Icones invalidos'; end if;
  clean = clean || jsonb_build_object(k, n::int);
 end loop;
 perform geobr_private.profile_sync_v2(p_name, p_icon, p_color1, p_color2, coalesce(p_icon_slug, ''));
 update public.geobr_profiles set glow = coalesce(p_glow, false), glow_color = p_glow_color, icons = clean, updated_at = now()
  where user_id = auth.uid() and (glow, glow_color, icons) is distinct from (coalesce(p_glow, false), p_glow_color, clean);
 return geobr_private.state_v2();
end $$;
revoke all on function geobr_private.profile_sync_v3(text,integer,integer,integer,text,boolean,integer,jsonb) from public, anon;
grant execute on function geobr_private.profile_sync_v3(text,integer,integer,integer,text,boolean,integer,jsonb) to authenticated;
create or replace function public.geobr_profile_sync_v3(p_name text, p_icon integer, p_color1 integer, p_color2 integer,
  p_icon_slug text, p_glow boolean, p_glow_color integer, p_icons jsonb)
returns jsonb language sql set search_path = '' as $$ select geobr_private.profile_sync_v3(p_name,p_icon,p_color1,p_color2,p_icon_slug,p_glow,p_glow_color,p_icons) $$;
revoke all on function public.geobr_profile_sync_v3(text,integer,integer,integer,text,boolean,integer,jsonb) from public, anon;
grant execute on function public.geobr_profile_sync_v3(text,integer,integer,integer,text,boolean,integer,jsonb) to authenticated;

-- Public card fields of a profile (one place, used by every list below).
create or replace function geobr_private.player_json(p_user uuid)
returns jsonb language sql stable security definer set search_path = '' as $$
 select jsonb_build_object('user_id', p.user_id, 'display_name', p.display_name, 'title_name', p.title_name,
  'theme_slug', p.theme_slug, 'lifetime_xp', p.lifetime_xp, 'icon_id', p.icon_id, 'icon_slug', p.icon_slug,
  'color1', p.color1, 'color2', p.color2, 'glow', p.glow, 'glow_color', p.glow_color, 'icons', p.icons,
  'arrow_skin', p.arrow_skin)
 from public.geobr_profiles p where p.user_id = p_user
$$;
revoke all on function geobr_private.player_json(uuid) from public, anon, authenticated;

create or replace function public.geobr_players(p_query text default ''::text, p_offset integer default 0)
returns jsonb language plpgsql stable set search_path = '' as $$
declare result jsonb;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 if p_offset is null or p_offset not between 0 and 300000 or char_length(coalesce(p_query, '')) > 40 then raise exception 'Busca invalida'; end if;
 with matched as materialized (
  select user_id, display_name, lifetime_xp, theme_slug, title_name, icon_id, color1, color2, icon_slug, glow, glow_color, icons, arrow_skin
  from public.geobr_profiles where strpos(lower(display_name), lower(trim(coalesce(p_query, '')))) > 0
 ), page as (select * from matched order by lower(display_name), user_id limit 3 offset p_offset)
 select jsonb_build_object('players', coalesce((select jsonb_agg(to_jsonb(p) order by lower(p.display_name), p.user_id) from page p), '[]'::jsonb),
  'total', (select count(*) from matched)) into result;
 return result;
end $$;

-- 3) Groups.
create table if not exists geobr_private.groups (
 id uuid primary key default gen_random_uuid(),
 name text not null check (char_length(btrim(name)) between 3 and 32),
 owner uuid not null references auth.users(id) on delete cascade,
 created_at timestamptz not null default now());
create table if not exists geobr_private.group_members (
 group_id uuid not null references geobr_private.groups(id) on delete cascade,
 user_id uuid not null references auth.users(id) on delete cascade,
 role text not null default 'member' check (role in ('owner','member')),
 joined_at timestamptz not null default now(),
 primary key (group_id, user_id));
create table if not exists geobr_private.group_messages (
 id uuid primary key,
 group_id uuid not null references geobr_private.groups(id) on delete cascade,
 user_id uuid not null references auth.users(id) on delete cascade,
 body text not null check (char_length(btrim(body)) between 1 and 300),
 created_at timestamptz not null default now());
create index if not exists geobr_groups_owner_idx on geobr_private.groups(owner);
create index if not exists geobr_group_members_user_idx on geobr_private.group_members(user_id);
create index if not exists geobr_group_messages_group_idx on geobr_private.group_messages(group_id, created_at desc);
create index if not exists geobr_group_messages_user_idx on geobr_private.group_messages(user_id, created_at desc);
alter table geobr_private.groups enable row level security;
alter table geobr_private.group_members enable row level security;
alter table geobr_private.group_messages enable row level security;

create or replace function geobr_private.group_member_or_fail(p_group uuid)
returns text language plpgsql stable security definer set search_path = '' as $$
declare r text;
begin
 select role into r from geobr_private.group_members where group_id = p_group and user_id = auth.uid();
 if r is null then raise exception 'Voce nao esta neste grupo'; end if;
 return r;
end $$;
revoke all on function geobr_private.group_member_or_fail(uuid) from public, anon, authenticated;

create or replace function geobr_private.group_create(p_name text)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare g uuid; u uuid = auth.uid();
begin
 perform geobr_private.require_access();
 if p_name is null or char_length(btrim(p_name)) not between 3 and 32 or p_name ~ '[[:cntrl:]]' then raise exception 'Nome do grupo: 3 a 32 caracteres'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-group:' || u::text, 0));
 if (select count(*) from geobr_private.groups where owner = u) >= 10 then raise exception 'Limite de 10 grupos criados'; end if;
 if (select count(*) from geobr_private.group_members where user_id = u) >= 30 then raise exception 'Limite de 30 grupos'; end if;
 insert into geobr_private.groups(name, owner) values (btrim(p_name), u) returning id into g;
 insert into geobr_private.group_members(group_id, user_id, role) values (g, u, 'owner');
 return jsonb_build_object('id', g, 'name', btrim(p_name), 'role', 'owner');
end $$;

create or replace function geobr_private.group_list()
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 return coalesce((select jsonb_agg(x order by x->>'last_at' desc nulls last, x->>'name') from (
  select jsonb_build_object('id', g.id, 'name', g.name, 'role', m.role,
   'members', (select count(*) from geobr_private.group_members c where c.group_id = g.id),
   'last_body', (select left(t.body, 60) from geobr_private.group_messages t where t.group_id = g.id order by t.created_at desc limit 1),
   'last_at', (select max(t.created_at) from geobr_private.group_messages t where t.group_id = g.id)) x
  from geobr_private.group_members m join geobr_private.groups g on g.id = m.group_id where m.user_id = auth.uid()) s), '[]'::jsonb);
end $$;

create or replace function geobr_private.group_member_list(p_group uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.group_member_or_fail(p_group);
 return coalesce((select jsonb_agg(geobr_private.player_json(m.user_id) || jsonb_build_object('role', m.role) order by m.role <> 'owner', m.joined_at)
  from geobr_private.group_members m where m.group_id = p_group), '[]'::jsonb);
end $$;

create or replace function geobr_private.group_add(p_group uuid, p_user uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); gname text; actor text;
begin
 perform geobr_private.require_access();
 perform geobr_private.group_member_or_fail(p_group);
 if p_user is null or p_user = u or not exists (select 1 from public.geobr_profiles where user_id = p_user) then raise exception 'Jogador nao encontrado'; end if;
 if exists (select 1 from public.geobr_dm_blocks where (owner = u and peer = p_user) or (owner = p_user and peer = u)) then raise exception 'Este jogador nao pode ser adicionado'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-group-add:' || p_group::text, 0));
 if exists (select 1 from geobr_private.group_members where group_id = p_group and user_id = p_user) then return jsonb_build_object('ok', true); end if;
 if (select count(*) from geobr_private.group_members where group_id = p_group) >= 30 then raise exception 'Grupo cheio (30)'; end if;
 if (select count(*) from geobr_private.group_members where user_id = p_user) >= 30 then raise exception 'Este jogador ja esta em 30 grupos'; end if;
 insert into geobr_private.group_members(group_id, user_id) values (p_group, p_user);
 select name into gname from geobr_private.groups where id = p_group;
 select display_name into actor from public.geobr_profiles where user_id = u;
 insert into public.geobr_social_notifications(owner, actor, kind, body) values (p_user, u, 'group', left(coalesce(actor, 'Jogador') || ' adicionou voce ao grupo ' || gname, 200));
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.group_remove(p_group uuid, p_user uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 if geobr_private.group_member_or_fail(p_group) <> 'owner' then raise exception 'Apenas o dono remove membros'; end if;
 if p_user = auth.uid() then raise exception 'Use Sair do grupo'; end if;
 delete from geobr_private.group_members where group_id = p_group and user_id = p_user;
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.group_leave(p_group uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare r text; heir uuid;
begin
 perform geobr_private.require_access();
 r = geobr_private.group_member_or_fail(p_group);
 delete from geobr_private.group_members where group_id = p_group and user_id = auth.uid();
 if r = 'owner' then
  select user_id into heir from geobr_private.group_members where group_id = p_group order by joined_at limit 1;
  if heir is null then delete from geobr_private.groups where id = p_group;
  else
   update geobr_private.group_members set role = 'owner' where group_id = p_group and user_id = heir;
   update geobr_private.groups set owner = heir where id = p_group;
  end if;
 end if;
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.group_chat(p_group uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.group_member_or_fail(p_group);
 return coalesce((select jsonb_agg(x order by x->>'created_at' desc) from (
  select jsonb_build_object('id', t.id, 'user_id', t.user_id, 'body', t.body, 'created_at', t.created_at,
   'sender_name', coalesce(p.display_name, 'Jogador'), 'icon_id', p.icon_id, 'icon_slug', p.icon_slug, 'color1', p.color1,
   'color2', p.color2, 'glow', p.glow, 'glow_color', p.glow_color) x
  from geobr_private.group_messages t left join public.geobr_profiles p on p.user_id = t.user_id
  where t.group_id = p_group order by t.created_at desc limit 50) s), '[]'::jsonb);
end $$;

create or replace function geobr_private.group_send(p_group uuid, p_body text, p_id uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); old geobr_private.group_messages;
begin
 perform geobr_private.require_access('chat');
 perform geobr_private.group_member_or_fail(p_group);
 if p_id is null then raise exception 'Mensagem invalida'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-social:' || u::text, 0));
 select * into old from geobr_private.group_messages where id = p_id;
 if found then
  if old.user_id = u and old.group_id = p_group and old.body = p_body then return jsonb_build_object('ok', true); end if;
  raise exception 'Identidade de mensagem invalida';
 end if;
 if p_body is null or char_length(btrim(p_body)) not between 1 and 300 or p_body ~ '[[:cntrl:]]' then raise exception 'Mensagem invalida'; end if;
 if exists (select 1 from geobr_private.group_messages where user_id = u and created_at > now() - interval '3 seconds') then raise exception 'Aguarde 3 segundos'; end if;
 if (select count(*) from geobr_private.group_messages where user_id = u and created_at > now() - interval '1 hour') >= 120 then raise exception 'Limite de mensagens atingido'; end if;
 insert into geobr_private.group_messages(id, group_id, user_id, body) values (p_id, p_group, u, btrim(p_body));
 return jsonb_build_object('ok', true);
end $$;

-- 4) Collab editor (tables from beta 1.3 are reused: rooms, members, snapshots).
alter table public.geobr_collab_rooms
  add column if not exists gd_level_id bigint,
  add column if not exists posted_at timestamptz,
  add column if not exists posted_by uuid references auth.users(id) on delete set null;
create unique index if not exists geobr_collab_rooms_gd_level_idx on public.geobr_collab_rooms(gd_level_id) where gd_level_id is not null;
create index if not exists geobr_collab_rooms_posted_by_idx on public.geobr_collab_rooms(posted_by);
alter table public.geobr_collab_rooms drop constraint if exists geobr_collab_rooms_gd_level_check;
alter table public.geobr_collab_rooms add constraint geobr_collab_rooms_gd_level_check check (gd_level_id is null or gd_level_id between 1 and 2000000000);
create index if not exists geobr_collab_members_user_idx on public.geobr_collab_members(user_id);

create table if not exists geobr_private.collab_messages (
 id uuid primary key,
 room_id uuid not null references public.geobr_collab_rooms(id) on delete cascade,
 user_id uuid not null references auth.users(id) on delete cascade,
 body text not null check (char_length(btrim(body)) between 1 and 200),
 created_at timestamptz not null default now());
create index if not exists geobr_collab_messages_room_idx on geobr_private.collab_messages(room_id, created_at desc);
create index if not exists geobr_collab_messages_user_idx on geobr_private.collab_messages(user_id, created_at desc);
create table if not exists geobr_private.collab_join_attempts (
 user_id uuid not null references auth.users(id) on delete cascade,
 attempted_at timestamptz not null default now(),
 ok boolean not null);
create index if not exists geobr_collab_join_attempts_idx on geobr_private.collab_join_attempts(user_id, attempted_at desc);
alter table geobr_private.collab_messages enable row level security;
alter table geobr_private.collab_join_attempts enable row level security;

create or replace function geobr_private.collab_role_or_fail(p_room uuid)
returns text language plpgsql stable security definer set search_path = '' as $$
declare r text;
begin
 select m.role into r from public.geobr_collab_members m join public.geobr_collab_rooms c on c.id = m.room_id
  where m.room_id = p_room and m.user_id = auth.uid() and c.active;
 if r is null then raise exception 'Voce nao esta nesta sala'; end if;
 return r;
end $$;
revoke all on function geobr_private.collab_role_or_fail(uuid) from public, anon, authenticated;

create or replace function geobr_private.collab_display_name()
returns text language sql stable security definer set search_path = '' as $$
 select left(coalesce(nullif(btrim((select display_name from public.geobr_profiles where user_id = auth.uid())), ''), 'Jogador GeoBR'), 32)
$$;
revoke all on function geobr_private.collab_display_name() from public, anon, authenticated;

create or replace function geobr_private.collab_room_json(p_room uuid)
returns jsonb language sql stable security definer set search_path = '' as $$
 select jsonb_build_object('id', c.id, 'code', c.code, 'title', c.title, 'owner_id', c.owner_id,
  'role', (select role from public.geobr_collab_members where room_id = c.id and user_id = auth.uid()),
  'revision', coalesce(s.revision, 0), 'updated_at', coalesce(s.updated_at, c.updated_at),
  'members', (select count(*) from public.geobr_collab_members where room_id = c.id),
  'gd_level_id', c.gd_level_id)
 from public.geobr_collab_rooms c left join public.geobr_collab_snapshots s on s.room_id = c.id where c.id = p_room
$$;
revoke all on function geobr_private.collab_room_json(uuid) from public, anon, authenticated;

create or replace function geobr_private.collab_create(p_title text, p_level text)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); alphabet text = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'; v_code text; raw bytea; i int; room uuid;
begin
 perform geobr_private.require_access();
 if p_title is null or char_length(btrim(p_title)) not between 1 and 40 or p_title ~ '[[:cntrl:]]' then raise exception 'Nome da sala: 1 a 40 caracteres'; end if;
 if octet_length(coalesce(p_level, '')) > 8388608 then raise exception 'Level grande demais (8 MiB)'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-collab:' || u::text, 0));
 if (select count(*) from public.geobr_collab_rooms where owner_id = u and active) >= 5 then raise exception 'Limite de 5 salas ativas'; end if;
 loop
  raw = uuid_send(gen_random_uuid()); v_code = '';
  for i in 0..7 loop v_code = v_code || substr(alphabet, (get_byte(raw, i) % 32) + 1, 1); end loop;
  exit when not exists (select 1 from public.geobr_collab_rooms r where r.code = v_code);
 end loop;
 insert into public.geobr_collab_rooms(code, title, owner_id) values (v_code, btrim(p_title), u) returning id into room;
 insert into public.geobr_collab_members(room_id, user_id, role, display_name) values (room, u, 'owner', geobr_private.collab_display_name());
 insert into public.geobr_collab_snapshots(room_id, revision, level_string, updated_by)
  values (room, case when coalesce(p_level, '') = '' then 0 else 1 end, coalesce(p_level, ''), u);
 return geobr_private.collab_room_json(room);
end $$;

create or replace function geobr_private.collab_join(p_code text)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); room public.geobr_collab_rooms;
begin
 perform geobr_private.require_access();
 perform pg_advisory_xact_lock(hashtextextended('geobr-collab-join:' || u::text, 0));
 if (select count(*) from geobr_private.collab_join_attempts where user_id = u and not ok and attempted_at > now() - interval '1 hour') >= 10 then
  return jsonb_build_object('ok', false, 'error', 'Muitas tentativas. Tente de novo em 1 hora.');
 end if;
 select * into room from public.geobr_collab_rooms where code = upper(btrim(coalesce(p_code, ''))) and active;
 if room.id is null then
  insert into geobr_private.collab_join_attempts(user_id, ok) values (u, false);
  return jsonb_build_object('ok', false, 'error', 'Codigo nao encontrado.');
 end if;
 if not exists (select 1 from public.geobr_collab_members where room_id = room.id and user_id = u) then
  if (select count(*) from public.geobr_collab_members where room_id = room.id) >= 8 then
   return jsonb_build_object('ok', false, 'error', 'Sala cheia (8 pessoas).');
  end if;
  if (select count(*) from public.geobr_collab_members where user_id = u) >= 20 then
   return jsonb_build_object('ok', false, 'error', 'Voce ja esta em 20 salas.');
  end if;
  insert into public.geobr_collab_members(room_id, user_id, role, display_name) values (room.id, u, 'editor', geobr_private.collab_display_name());
  insert into public.geobr_social_notifications(owner, actor, kind, body)
   select m.user_id, u, 'collab', left(geobr_private.collab_display_name() || ' entrou no collab ' || room.title, 200)
   from public.geobr_collab_members m where m.room_id = room.id and m.user_id <> u;
 end if;
 insert into geobr_private.collab_join_attempts(user_id, ok) values (u, true);
 return jsonb_build_object('ok', true, 'room', geobr_private.collab_room_json(room.id));
end $$;

create or replace function geobr_private.collab_rooms()
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 return coalesce((select jsonb_agg(geobr_private.collab_room_json(c.id) order by c.updated_at desc)
  from public.geobr_collab_rooms c join public.geobr_collab_members m on m.room_id = c.id
  where m.user_id = auth.uid() and c.active), '[]'::jsonb);
end $$;

create or replace function geobr_private.collab_pull(p_room uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
declare s public.geobr_collab_snapshots;
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 select * into s from public.geobr_collab_snapshots where room_id = p_room;
 return geobr_private.collab_room_json(p_room) || jsonb_build_object('level', coalesce(s.level_string, ''),
  'updated_by_name', (select display_name from public.geobr_profiles where user_id = s.updated_by));
end $$;

create or replace function geobr_private.collab_push(p_room uuid, p_level text, p_base bigint)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare s public.geobr_collab_snapshots; r text;
begin
 perform geobr_private.require_access();
 r = geobr_private.collab_role_or_fail(p_room);
 if r not in ('owner', 'editor') then raise exception 'Somente leitura nesta sala'; end if;
 if p_level is null or octet_length(p_level) > 8388608 then raise exception 'Level grande demais (8 MiB)'; end if;
 select * into s from public.geobr_collab_snapshots where room_id = p_room for update;
 if not found then
  insert into public.geobr_collab_snapshots(room_id, revision, level_string, updated_by) values (p_room, 0, '', auth.uid()) returning * into s;
 end if;
 if s.revision <> coalesce(p_base, -1) then
  return jsonb_build_object('ok', false, 'conflict', true, 'revision', s.revision, 'level', s.level_string);
 end if;
 update public.geobr_collab_snapshots set revision = s.revision + 1, level_string = p_level, updated_by = auth.uid(), updated_at = now()
  where room_id = p_room;
 update public.geobr_collab_rooms set updated_at = now() where id = p_room;
 return jsonb_build_object('ok', true, 'revision', s.revision + 1);
end $$;

create or replace function geobr_private.collab_member_list(p_room uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 return coalesce((select jsonb_agg(coalesce(geobr_private.player_json(m.user_id), jsonb_build_object('user_id', m.user_id, 'display_name', m.display_name))
  || jsonb_build_object('role', m.role) order by m.role <> 'owner', m.joined_at)
  from public.geobr_collab_members m where m.room_id = p_room), '[]'::jsonb);
end $$;

create or replace function geobr_private.collab_leave(p_room uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare r text; heir uuid;
begin
 perform geobr_private.require_access();
 r = geobr_private.collab_role_or_fail(p_room);
 delete from public.geobr_collab_members where room_id = p_room and user_id = auth.uid();
 if r = 'owner' then
  select user_id into heir from public.geobr_collab_members where room_id = p_room order by joined_at limit 1;
  if heir is null then update public.geobr_collab_rooms set active = false, updated_at = now() where id = p_room;
  else
   update public.geobr_collab_members set role = 'owner' where room_id = p_room and user_id = heir;
   update public.geobr_collab_rooms set owner_id = heir, updated_at = now() where id = p_room;
  end if;
 end if;
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.collab_chat(p_room uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 return coalesce((select jsonb_agg(x order by x->>'created_at' desc) from (
  select jsonb_build_object('id', t.id, 'user_id', t.user_id, 'body', t.body, 'created_at', t.created_at,
   'sender_name', coalesce(p.display_name, 'Jogador'), 'icon_id', p.icon_id, 'icon_slug', p.icon_slug,
   'color1', p.color1, 'color2', p.color2, 'glow', p.glow, 'glow_color', p.glow_color) x
  from geobr_private.collab_messages t left join public.geobr_profiles p on p.user_id = t.user_id
  where t.room_id = p_room order by t.created_at desc limit 50) s), '[]'::jsonb);
end $$;

create or replace function geobr_private.collab_send(p_room uuid, p_body text, p_id uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); old geobr_private.collab_messages;
begin
 perform geobr_private.require_access('chat');
 perform geobr_private.collab_role_or_fail(p_room);
 if p_id is null then raise exception 'Mensagem invalida'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-social:' || u::text, 0));
 select * into old from geobr_private.collab_messages where id = p_id;
 if found then
  if old.user_id = u and old.room_id = p_room and old.body = p_body then return jsonb_build_object('ok', true); end if;
  raise exception 'Identidade de mensagem invalida';
 end if;
 if p_body is null or char_length(btrim(p_body)) not between 1 and 200 or p_body ~ '[[:cntrl:]]' then raise exception 'Mensagem invalida'; end if;
 if exists (select 1 from geobr_private.collab_messages where user_id = u and created_at > now() - interval '2 seconds') then raise exception 'Aguarde 2 segundos'; end if;
 if (select count(*) from geobr_private.collab_messages where user_id = u and created_at > now() - interval '1 hour') >= 180 then raise exception 'Limite de mensagens atingido'; end if;
 insert into geobr_private.collab_messages(id, room_id, user_id, body) values (p_id, p_room, u, btrim(p_body));
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.collab_publish(p_room uuid, p_level_id bigint)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); room public.geobr_collab_rooms;
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 if p_level_id is null or p_level_id not between 1 and 2000000000 then raise exception 'ID de level invalido'; end if;
 select * into room from public.geobr_collab_rooms where id = p_room for update;
 if room.gd_level_id = p_level_id then return jsonb_build_object('ok', true); end if;
 if exists (select 1 from public.geobr_collab_rooms where gd_level_id = p_level_id and id <> p_room) then raise exception 'Este level ja esta ligado a outro collab'; end if;
 update public.geobr_collab_rooms set gd_level_id = p_level_id, posted_at = now(), posted_by = u, updated_at = now() where id = p_room;
 insert into public.geobr_social_notifications(owner, actor, kind, body)
  select m.user_id, u, 'collab', left(geobr_private.collab_display_name() || ' publicou o collab ' || room.title || ' (ID ' || p_level_id || ')', 200)
  from public.geobr_collab_members m where m.room_id = p_room and m.user_id <> u;
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.collab_published()
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 return coalesce((select jsonb_agg(gd_level_id order by gd_level_id) from (
  select gd_level_id from public.geobr_collab_rooms where gd_level_id is not null order by posted_at desc limit 5000) s), '[]'::jsonb);
end $$;

create or replace function geobr_private.collab_credits(p_level_id bigint)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
declare room public.geobr_collab_rooms;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 select * into room from public.geobr_collab_rooms where gd_level_id = p_level_id;
 if room.id is null then return jsonb_build_object('found', false); end if;
 return jsonb_build_object('found', true, 'title', room.title, 'posted_at', room.posted_at,
  'members', coalesce((select jsonb_agg(coalesce(geobr_private.player_json(m.user_id), jsonb_build_object('user_id', m.user_id, 'display_name', m.display_name))
   || jsonb_build_object('role', m.role) order by m.role <> 'owner', m.joined_at)
   from public.geobr_collab_members m where m.room_id = room.id), '[]'::jsonb));
end $$;

-- Grants and public wrappers (invoker SQL functions, authenticated only).
do $$
declare f record;
begin
 for f in select * from (values
  ('group_create', 'group_create', 'p_name text', 'p_name'),
  ('group_list', 'group_list', '', ''),
  ('group_member_list', 'group_members', 'p_group uuid', 'p_group'),
  ('group_add', 'group_add', 'p_group uuid, p_user uuid', 'p_group, p_user'),
  ('group_remove', 'group_remove', 'p_group uuid, p_user uuid', 'p_group, p_user'),
  ('group_leave', 'group_leave', 'p_group uuid', 'p_group'),
  ('group_chat', 'group_messages', 'p_group uuid', 'p_group'),
  ('group_send', 'group_send', 'p_group uuid, p_body text, p_id uuid', 'p_group, p_body, p_id'),
  ('collab_create', 'collab_create', 'p_title text, p_level text', 'p_title, p_level'),
  ('collab_join', 'collab_join', 'p_code text', 'p_code'),
  ('collab_rooms', 'collab_rooms', '', ''),
  ('collab_pull', 'collab_pull', 'p_room uuid', 'p_room'),
  ('collab_push', 'collab_push', 'p_room uuid, p_level text, p_base bigint', 'p_room, p_level, p_base'),
  ('collab_member_list', 'collab_members', 'p_room uuid', 'p_room'),
  ('collab_leave', 'collab_leave', 'p_room uuid', 'p_room'),
  ('collab_chat', 'collab_messages', 'p_room uuid', 'p_room'),
  ('collab_send', 'collab_send', 'p_room uuid, p_body text, p_id uuid', 'p_room, p_body, p_id'),
  ('collab_publish', 'collab_publish', 'p_room uuid, p_level_id bigint', 'p_room, p_level_id'),
  ('collab_published', 'collab_published', '', ''),
  ('collab_credits', 'collab_credits', 'p_level_id bigint', 'p_level_id')) as t(name, pub, sig, args)
 loop
  execute format('revoke all on function geobr_private.%I(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function geobr_private.%I(%s) to authenticated', f.name, f.sig);
  execute format('create or replace function public.geobr_%s(%s) returns jsonb language sql set search_path = '''' as $f$ select geobr_private.%I(%s) $f$',
   f.pub, f.sig, f.name, f.args);
  execute format('revoke all on function public.geobr_%s(%s) from public, anon', f.pub, f.sig);
  execute format('grant execute on function public.geobr_%s(%s) to authenticated', f.pub, f.sig);
 end loop;
end $$;

-- 5) RC9 release row.
insert into geobr_private.releases(version, enabled, message)
values ('1.9.0-beta.9', true, 'GeoBR RC9')
on conflict (version) do nothing;
