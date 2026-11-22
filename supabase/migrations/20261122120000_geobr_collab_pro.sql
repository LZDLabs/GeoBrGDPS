-- GeoBR 1.17.0: professional collab rooms.
--  * The yellow teleport portal (GD object 749, the exit GD creates by itself
--    with every blue teleport portal) is never stored as a live object: it was
--    sent as a new object by every editor and multiplied without end.
--  * Change history per room (ring of the last 30000 changes) so the host and
--    the ADMs can revert a member's edits or the whole room for N minutes.
--  * Moderation (kick / ban in 20261122130000): unban, mute in chat, view only, lock the room,
--    new code, resync everyone, clean yellow portals / exact duplicates,
--    restore the room from a level (last save or empty).
--  * The live round (collab_live2) also returns the new chat messages, the
--    room generation (a change makes every editor reload the room) and why a
--    member was removed.
-- Non-destructive: new columns, tables and functions; replaced functions keep
-- their signatures.

alter table public.geobr_collab_rooms
  add column if not exists live_gen integer not null default 0,
  add column if not exists locked boolean not null default false;
alter table public.geobr_collab_members add column if not exists muted boolean not null default false;
alter table geobr_private.collab_messages add column if not exists kind text not null default 'user';
do $$ begin
 if not exists (select 1 from pg_constraint where conname = 'collab_messages_kind_check') then
  alter table geobr_private.collab_messages add constraint collab_messages_kind_check check (kind in ('user', 'system'));
 end if;
end $$;

create table if not exists geobr_private.collab_history (
  room_id uuid not null references public.geobr_collab_rooms(id) on delete cascade,
  slot integer not null,
  seq bigint not null,
  uid text not null,
  prev text,
  data text,
  user_id uuid references auth.users(id) on delete set null,
  at timestamptz not null default now(),
  primary key (room_id, slot));
create index if not exists geobr_collab_history_at_idx on geobr_private.collab_history(room_id, at);
create index if not exists geobr_collab_history_uid_idx on geobr_private.collab_history(room_id, uid, seq);
create index if not exists geobr_collab_history_user_idx on geobr_private.collab_history(user_id);

create table if not exists geobr_private.collab_bans (
  room_id uuid not null references public.geobr_collab_rooms(id) on delete cascade,
  user_id uuid not null references auth.users(id) on delete cascade,
  display_name text not null default 'Jogador',
  banned_by uuid references auth.users(id) on delete set null,
  active boolean not null default true,
  created_at timestamptz not null default now(),
  primary key (room_id, user_id));
create index if not exists geobr_collab_bans_user_idx on geobr_private.collab_bans(user_id);
create index if not exists geobr_collab_bans_by_idx on geobr_private.collab_bans(banned_by);

create table if not exists geobr_private.collab_mod_log (
  id bigint generated always as identity primary key,
  room_id uuid not null references public.geobr_collab_rooms(id) on delete cascade,
  actor uuid references auth.users(id) on delete set null,
  target uuid references auth.users(id) on delete set null,
  action text not null,
  detail text not null default '',
  at timestamptz not null default now());
create index if not exists geobr_collab_mod_log_room_idx on geobr_private.collab_mod_log(room_id, at desc);
create index if not exists geobr_collab_mod_log_target_idx on geobr_private.collab_mod_log(target, at desc);
create index if not exists geobr_collab_mod_log_actor_idx on geobr_private.collab_mod_log(actor);

alter table geobr_private.collab_history enable row level security;
alter table geobr_private.collab_bans enable row level security;
alter table geobr_private.collab_mod_log enable row level security;

-- A GD object string of the yellow teleport portal.
create or replace function geobr_private.collab_is_yellow(p_data text)
returns boolean language sql immutable set search_path = '' as $$
 select coalesce(p_data ~ '^1,749(,|$)', false)
$$;
revoke all on function geobr_private.collab_is_yellow(text) from public, anon, authenticated;

create or replace function geobr_private.collab_name_of(p_user uuid)
returns text language sql stable security definer set search_path = '' as $$
 select left(coalesce(nullif(btrim((select display_name from public.geobr_profiles where user_id = p_user)), ''),
  (select display_name from public.geobr_collab_members where user_id = p_user limit 1), 'Jogador'), 32)
$$;
revoke all on function geobr_private.collab_name_of(uuid) from public, anon, authenticated;

-- 3 = host, 2 = ADM, 1 = editor / view only. Raises unless the caller is the
-- host or an ADM; with a target, the caller must rank above it. A target who
-- already left can still have edits reverted (p_allow_gone).
create or replace function geobr_private.collab_mod_check(p_room uuid, p_target uuid, p_allow_gone boolean default false)
returns integer language plpgsql stable security definer set search_path = '' as $$
declare me public.geobr_collab_members; t public.geobr_collab_members; mine int; theirs int;
begin
 perform geobr_private.collab_role_or_fail(p_room);
 select * into me from public.geobr_collab_members where room_id = p_room and user_id = auth.uid();
 mine = case when me.role = 'owner' then 3 when me.can_save then 2 else 1 end;
 if mine < 2 then raise exception 'Somente o host e os ADMs moderam a sala'; end if;
 if p_target is null then return mine; end if;
 if p_target = auth.uid() then raise exception 'Voce nao pode fazer isso com voce mesmo'; end if;
 select * into t from public.geobr_collab_members where room_id = p_room and user_id = p_target;
 if t.user_id is null then
  if p_allow_gone then return mine; end if;
  raise exception 'Membro nao encontrado';
 end if;
 theirs = case when t.role = 'owner' then 3 when t.can_save then 2 else 1 end;
 if theirs >= mine then raise exception 'Voce nao pode moderar esse membro'; end if;
 return mine;
end $$;
revoke all on function geobr_private.collab_mod_check(uuid, uuid, boolean) from public, anon, authenticated;

create or replace function geobr_private.collab_note(p_room uuid, p_target uuid, p_action text, p_detail text, p_chat text)
returns void language plpgsql security definer set search_path = '' as $$
begin
 insert into geobr_private.collab_mod_log(room_id, actor, target, action, detail) values (p_room, auth.uid(), p_target, p_action, left(coalesce(p_detail, ''), 200));
 if coalesce(p_chat, '') <> '' then
  insert into geobr_private.collab_messages(id, room_id, user_id, body, kind)
  values (gen_random_uuid(), p_room, auth.uid(), left(regexp_replace(p_chat, '[[:cntrl:]]', ' ', 'g'), 200), 'system');
 end if;
end $$;
revoke all on function geobr_private.collab_note(uuid, uuid, text, text, text) from public, anon, authenticated;

-- Writes many live objects at once (null data = deleted), with history, as a
-- change of nobody (updated_by null), so every editor applies it, the
-- caller's included. The uids must be distinct.
create or replace function geobr_private.collab_bulk(p_room uuid, p_uids text[], p_datas text[])
returns integer language plpgsql security definer set search_path = '' as $$
declare n int = coalesce(cardinality(p_uids), 0); s0 bigint;
begin
 if n = 0 then return 0; end if;
 update public.geobr_collab_rooms set live_seq = live_seq + n, updated_at = now() where id = p_room returning live_seq - n into s0;
 if n <= 30000 then
  insert into geobr_private.collab_history(room_id, slot, seq, uid, prev, data, user_id, at)
   select p_room, ((s0 + x.i) % 30000)::int, s0 + x.i, x.u, o.data, x.d, auth.uid(), now()
   from unnest(p_uids, p_datas) with ordinality as x(u, d, i)
   left join public.geobr_collab_objects o on o.room_id = p_room and o.uid = x.u
  on conflict (room_id, slot) do update set seq = excluded.seq, uid = excluded.uid, prev = excluded.prev,
   data = excluded.data, user_id = excluded.user_id, at = excluded.at;
 end if;
 insert into public.geobr_collab_objects(room_id, uid, data, seq, updated_by)
  select p_room, x.u, x.d, s0 + x.i, null from unnest(p_uids, p_datas) with ordinality as x(u, d, i)
 on conflict (room_id, uid) do update set data = excluded.data, seq = excluded.seq, updated_by = null;
 return n;
end $$;
revoke all on function geobr_private.collab_bulk(uuid, text[], text[]) from public, anon, authenticated;

-- The live round, now with history and without yellow portals.
create or replace function geobr_private.collab_live(p_room uuid, p_since bigint, p_ops jsonb, p_x real, p_y real)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare r text; u uuid = auth.uid(); n int; s bigint; e jsonb; upto bigint; ops jsonb; saver boolean; created int = 0; was boolean; prev text;
 settings boolean = geobr_private.collab_client_has_settings();
begin
 perform geobr_private.require_access();
 r = geobr_private.collab_role_or_fail(p_room);
 if p_ops is null or jsonb_typeof(p_ops) <> 'array' then p_ops = '[]'::jsonb; end if;
 n = jsonb_array_length(p_ops);
 if n > 0 then
  if r not in ('owner', 'editor') then raise exception 'Somente leitura nesta sala'; end if;
  if n > 2000 then raise exception 'Muitas alteracoes de uma vez'; end if;
  saver = geobr_private.collab_can_save(p_room);
  update public.geobr_collab_rooms set live_seq = live_seq + n, updated_at = now() where id = p_room returning live_seq - n into s;
  for e in select * from jsonb_array_elements(p_ops) loop
   s = s + 1;
   if coalesce(e->>'u', '') !~ '^[A-Za-z0-9_.-]{1,40}$' or octet_length(coalesce(e->>'d', '')) > 16384 then continue; end if;
   if left(e->>'u', 2) = '__' and not saver then continue; end if;
   -- The yellow teleport portal comes with its blue portal: never its own object.
   if geobr_private.collab_is_yellow(e->>'d') then continue; end if;
   select o.data into prev from public.geobr_collab_objects o where o.room_id = p_room and o.uid = e->>'u';
   was = prev is not null;
   insert into public.geobr_collab_objects(room_id, uid, data, seq, updated_by) values (p_room, e->>'u', e->>'d', s, u)
    on conflict (room_id, uid) do update set data = excluded.data, seq = excluded.seq, updated_by = excluded.updated_by;
   if left(e->>'u', 2) <> '__' then
    insert into geobr_private.collab_history(room_id, slot, seq, uid, prev, data, user_id, at)
     values (p_room, (s % 30000)::int, s, e->>'u', prev, e->>'d', u, now())
    on conflict (room_id, slot) do update set seq = excluded.seq, uid = excluded.uid, prev = excluded.prev,
     data = excluded.data, user_id = excluded.user_id, at = excluded.at;
   end if;
   if not was and e->>'d' is not null and left(e->>'u', 2) <> '__' then created = created + 1; end if;
  end loop;
  if created > 0 then perform geobr_private.social_progress(u, 'collab_objects', p_room::text || ':' || s::text, created); end if;
 end if;
 insert into public.geobr_collab_presence(room_id, user_id, display_name, x, y, updated_at)
  values (p_room, u, geobr_private.collab_display_name(), coalesce(p_x, 0), coalesce(p_y, 0), now())
  on conflict (room_id, user_id) do update set display_name = excluded.display_name, x = excluded.x, y = excluded.y, updated_at = now();
 select coalesce(jsonb_agg(jsonb_build_object('u', o.uid, 'd', o.data, 'mine', o.updated_by = u) order by o.seq)
   filter (where settings or left(o.uid, 2) <> '__'), '[]'::jsonb), max(o.seq)
  into ops, upto
  from (select * from public.geobr_collab_objects where room_id = p_room and seq > coalesce(p_since, 0) order by seq limit 3000) o;
 return jsonb_build_object(
  'seq', coalesce(upto, greatest(coalesce(p_since, 0), (select live_seq from public.geobr_collab_rooms where id = p_room))),
  'more', upto is not null and upto < (select live_seq from public.geobr_collab_rooms where id = p_room),
  'can_save', geobr_private.collab_can_save(p_room),
  'ops', ops,
  'presence', coalesce((select jsonb_agg(jsonb_build_object('name', p.display_name, 'x', p.x, 'y', p.y))
    from public.geobr_collab_presence p where p.room_id = p_room and p.user_id <> u and p.updated_at > now() - interval '12 seconds'), '[]'::jsonb));
end $$;

-- Seeding never stores yellow portals either.
create or replace function geobr_private.collab_live_seed(p_room uuid, p_objects jsonb)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare r text; c public.geobr_collab_rooms; n int; saver boolean;
begin
 perform geobr_private.require_access();
 r = geobr_private.collab_role_or_fail(p_room);
 if r not in ('owner', 'editor') then raise exception 'Somente leitura nesta sala'; end if;
 if jsonb_typeof(p_objects) <> 'array' or jsonb_array_length(p_objects) > 100000 then raise exception 'Level invalido'; end if;
 select * into c from public.geobr_collab_rooms where id = p_room for update;
 if c.live_seq <> 0 or exists (select 1 from public.geobr_collab_objects where room_id = p_room) then
  return jsonb_build_object('ok', false);
 end if;
 saver = geobr_private.collab_can_save(p_room);
 n = jsonb_array_length(p_objects);
 insert into public.geobr_collab_objects(room_id, uid, data, seq, updated_by)
  select p_room, e->>'u', e->>'d', 1, auth.uid() from jsonb_array_elements(p_objects) e
  where e->>'d' is not null and not geobr_private.collab_is_yellow(e->>'d') and (saver or left(e->>'u', 2) <> '__')
    and coalesce(e->>'u', '') ~ '^[A-Za-z0-9_.-]{1,40}$' and octet_length(e->>'d') <= 16384
  on conflict do nothing;
 update public.geobr_collab_rooms set live_seq = 1, updated_at = now() where id = p_room;
 return jsonb_build_object('ok', true, 'seq', 1, 'objects', n);
end $$;

-- The 1.17 live round: the round above plus the room generation, the caller's
-- role, new chat messages since p_chat and why a removed member is out.
create or replace function geobr_private.collab_live2(p_room uuid, p_since bigint, p_ops jsonb, p_x real, p_y real, p_chat timestamptz)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); c public.geobr_collab_rooms; m public.geobr_collab_members; why text; res jsonb;
begin
 perform geobr_private.require_access();
 select * into c from public.geobr_collab_rooms where id = p_room;
 select * into m from public.geobr_collab_members where room_id = p_room and user_id = u;
 if c.id is null or not c.active or m.user_id is null then
  if c.id is null or not c.active then why = 'closed';
  elsif exists (select 1 from geobr_private.collab_bans where room_id = p_room and user_id = u and active) then why = 'ban';
  else
   select l.action into why from geobr_private.collab_mod_log l where l.room_id = p_room and l.target = u and l.action in ('kick', 'ban')
    order by l.at desc limit 1;
   why = coalesce(why, 'left');
  end if;
  return jsonb_build_object('removed', true, 'reason', why, 'title', c.title);
 end if;
 if m.role not in ('owner', 'editor') then p_ops = '[]'::jsonb; end if;
 res = geobr_private.collab_live(p_room, p_since, p_ops, p_x, p_y);
 return res || jsonb_build_object(
  'gen', c.live_gen, 'role', m.role, 'owner', m.role = 'owner', 'muted', m.muted, 'locked', c.locked,
  'chat_at', (select max(t.created_at) from geobr_private.collab_messages t where t.room_id = p_room),
  'chat', case when p_chat is null then '[]'::jsonb else coalesce((select jsonb_agg(x order by x->>'created_at') from (
    select jsonb_build_object('id', t.id, 'body', t.body, 'kind', t.kind, 'created_at', t.created_at,
     'sender_name', coalesce(p.display_name, 'Jogador'), 'icon_id', p.icon_id, 'icon_slug', p.icon_slug,
     'color1', p.color1, 'color2', p.color2, 'glow', p.glow, 'glow_color', p.glow_color) x
    from geobr_private.collab_messages t left join public.geobr_profiles p on p.user_id = t.user_id
    where t.room_id = p_room and t.created_at > p_chat and t.user_id <> u
    order by t.created_at desc limit 5) s), '[]'::jsonb) end);
end $$;

-- Chat: muted members cannot send.
create or replace function geobr_private.collab_send(p_room uuid, p_body text, p_id uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); old geobr_private.collab_messages;
begin
 perform geobr_private.require_access('chat');
 perform geobr_private.collab_role_or_fail(p_room);
 if exists (select 1 from public.geobr_collab_members where room_id = p_room and user_id = u and muted) then
  raise exception 'Voce foi silenciado no chat desta sala';
 end if;
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

create or replace function geobr_private.collab_chat(p_room uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 return coalesce((select jsonb_agg(x order by x->>'created_at' desc) from (
  select jsonb_build_object('id', t.id, 'user_id', t.user_id, 'body', t.body, 'kind', t.kind, 'created_at', t.created_at,
   'sender_name', coalesce(p.display_name, 'Jogador'), 'icon_id', p.icon_id, 'icon_slug', p.icon_slug,
   'color1', p.color1, 'color2', p.color2, 'glow', p.glow, 'glow_color', p.glow_color) x
  from geobr_private.collab_messages t left join public.geobr_profiles p on p.user_id = t.user_id
  where t.room_id = p_room order by t.created_at desc limit 50) s), '[]'::jsonb);
end $$;

create or replace function geobr_private.collab_member_list(p_room uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 return coalesce((select jsonb_agg(coalesce(geobr_private.player_json(m.user_id), jsonb_build_object('user_id', m.user_id, 'display_name', m.display_name))
  || jsonb_build_object('role', m.role, 'can_save', m.role = 'owner' or m.can_save, 'muted', m.muted) order by m.role <> 'owner', m.joined_at)
  from public.geobr_collab_members m where m.room_id = p_room), '[]'::jsonb);
end $$;

-- Joining: banned players and locked rooms stay out.
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
  if exists (select 1 from geobr_private.collab_bans where room_id = room.id and user_id = u and active) then
   return jsonb_build_object('ok', false, 'error', 'Voce foi banido desta sala.');
  end if;
  if room.locked then
   return jsonb_build_object('ok', false, 'error', 'A sala esta trancada pelo host.');
  end if;
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

create or replace function geobr_private.collab_unban(p_room uuid, p_user uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_mod_check(p_room, null);
 update geobr_private.collab_bans set active = false where room_id = p_room and user_id = p_user and active;
 if not found then raise exception 'Esse jogador nao esta banido'; end if;
 perform geobr_private.collab_note(p_room, p_user, 'unban', '', format('%s desbaniu %s', geobr_private.collab_display_name(), geobr_private.collab_name_of(p_user)));
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.collab_mute(p_room uuid, p_user uuid, p_on boolean)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_mod_check(p_room, p_user);
 update public.geobr_collab_members set muted = coalesce(p_on, false) where room_id = p_room and user_id = p_user;
 perform geobr_private.collab_note(p_room, p_user, case when p_on then 'mute' else 'unmute' end, '',
  format('%s %s %s no chat', geobr_private.collab_display_name(), case when p_on then 'silenciou' else 'liberou' end, geobr_private.collab_name_of(p_user)));
 return jsonb_build_object('ok', true);
end $$;

-- View only: the member keeps watching the room but cannot change it.
create or replace function geobr_private.collab_set_viewer(p_room uuid, p_user uuid, p_on boolean)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_mod_check(p_room, p_user);
 update public.geobr_collab_members set role = case when p_on then 'viewer' else 'editor' end,
  can_save = case when p_on then false else can_save end
  where room_id = p_room and user_id = p_user and role <> 'owner';
 perform geobr_private.collab_note(p_room, p_user, case when p_on then 'viewer' else 'editor' end, '',
  format('%s %s a edicao de %s', geobr_private.collab_display_name(), case when p_on then 'bloqueou' else 'liberou' end, geobr_private.collab_name_of(p_user)));
 return jsonb_build_object('ok', true);
end $$;

-- Reverts the last p_minutes of changes: of one member (p_user), skipping
-- objects someone else changed after them, or of everyone (p_user null).
create or replace function geobr_private.collab_revert(p_room uuid, p_user uuid, p_minutes integer)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare uids text[]; datas text[]; n int; skipped int; since timestamptz;
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_mod_check(p_room, p_user, true);
 if p_minutes is null or p_minutes not between 1 and 10080 then raise exception 'Periodo invalido'; end if;
 since = now() - make_interval(mins => p_minutes);
 perform 1 from public.geobr_collab_rooms where id = p_room for update;
 with firsts as (
   select distinct on (h.uid) h.uid, h.prev from geobr_private.collab_history h
   where h.room_id = p_room and h.at >= since and left(h.uid, 2) <> '__' and (p_user is null or h.user_id = p_user)
   order by h.uid, h.seq),
  lasts as (
   select distinct on (h.uid) h.uid, h.user_id from geobr_private.collab_history h
   where h.room_id = p_room and h.uid in (select f.uid from firsts f) order by h.uid, h.seq desc),
  pick as (
   select f.uid, f.prev, (p_user is null or l.user_id is not distinct from p_user) as ok from firsts f join lasts l on l.uid = f.uid)
 select array_agg(p.uid) filter (where p.ok and o.data is distinct from p.prev),
        array_agg(p.prev) filter (where p.ok and o.data is distinct from p.prev),
        count(*) filter (where not p.ok)
  into uids, datas, skipped
  from pick p left join public.geobr_collab_objects o on o.room_id = p_room and o.uid = p.uid;
 n = geobr_private.collab_bulk(p_room, uids, datas);
 if n > 1500 then update public.geobr_collab_rooms set live_gen = live_gen + 1 where id = p_room; end if;
 if n > 0 then
  perform geobr_private.collab_note(p_room, p_user, 'revert', format('%s min, %s objetos', p_minutes, n),
   case when p_user is null then format('%s desfez os ultimos %s min da sala (%s objetos)', geobr_private.collab_display_name(), p_minutes, n)
   else format('%s reverteu as edicoes de %s (%s objetos)', geobr_private.collab_display_name(), geobr_private.collab_name_of(p_user), n) end);
 end if;
 return jsonb_build_object('ok', true, 'reverted', n, 'skipped', coalesce(skipped, 0),
  'history_from', (select min(h.at) from geobr_private.collab_history h where h.room_id = p_room));
end $$;

-- Cleaning: 'yellow' removes stored yellow portals, 'dupes' removes exact
-- copies (same object string, same place), keeping the oldest one.
create or replace function geobr_private.collab_clean(p_room uuid, p_what text)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare uids text[]; n int;
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_mod_check(p_room, null);
 perform 1 from public.geobr_collab_rooms where id = p_room for update;
 if p_what = 'yellow' then
  select array_agg(o.uid) into uids from public.geobr_collab_objects o
   where o.room_id = p_room and o.data is not null and geobr_private.collab_is_yellow(o.data);
 elsif p_what = 'dupes' then
  select array_agg(x.uid) into uids from (
   select o.uid, row_number() over (partition by o.data order by o.seq, o.uid) rn
   from public.geobr_collab_objects o where o.room_id = p_room and o.data is not null and left(o.uid, 2) <> '__') x
   where x.rn > 1;
 else raise exception 'Limpeza invalida';
 end if;
 n = geobr_private.collab_bulk(p_room, uids, array_fill(null::text, array[coalesce(cardinality(uids), 0)]));
 if n > 1500 then update public.geobr_collab_rooms set live_gen = live_gen + 1 where id = p_room; end if;
 if n > 0 then
  perform geobr_private.collab_note(p_room, null, 'clean_' || p_what, format('%s objetos', n),
   format('%s apagou %s %s', geobr_private.collab_display_name(), n, case when p_what = 'yellow' then 'portais amarelos' else 'objetos duplicados' end));
 end if;
 return jsonb_build_object('ok', true, 'removed', n);
end $$;

-- Host only: the room becomes this list of objects (the last save, or empty).
create or replace function geobr_private.collab_restore(p_room uuid, p_objects jsonb, p_label text)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare g int; olds text[]; news text[]; newd text[]; n int;
begin
 perform geobr_private.require_access();
 if geobr_private.collab_mod_check(p_room, null) < 3 then raise exception 'Somente o host reinicia a sala'; end if;
 if p_objects is null or jsonb_typeof(p_objects) <> 'array' or jsonb_array_length(p_objects) > 100000 then raise exception 'Level invalido'; end if;
 update public.geobr_collab_rooms set live_gen = live_gen + 1 where id = p_room returning live_gen into g;
 select array_agg(o.uid) into olds from public.geobr_collab_objects o
  where o.room_id = p_room and o.data is not null and left(o.uid, 2) <> '__';
 select array_agg(format('r%s.%s', g, x.i)), array_agg(x.d) into news, newd
  from jsonb_array_elements_text(p_objects) with ordinality as x(d, i)
  where octet_length(x.d) between 3 and 16384 and x.d ~ '^1,[0-9]+' and not geobr_private.collab_is_yellow(x.d);
 n = geobr_private.collab_bulk(p_room, coalesce(olds, '{}') || coalesce(news, '{}'),
  array_fill(null::text, array[coalesce(cardinality(olds), 0)]) || coalesce(newd, '{}'));
 perform geobr_private.collab_note(p_room, null, 'restore', format('%s: %s objetos', left(coalesce(p_label, ''), 40), coalesce(cardinality(news), 0)),
  format('%s reiniciou a sala (%s)', geobr_private.collab_display_name(), left(coalesce(nullif(p_label, ''), 'level'), 40)));
 return jsonb_build_object('ok', true, 'objects', coalesce(cardinality(news), 0), 'removed', coalesce(cardinality(olds), 0));
end $$;

-- Every open editor reloads the room (fixes anyone out of sync).
create or replace function geobr_private.collab_resync(p_room uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_mod_check(p_room, null);
 update public.geobr_collab_rooms set live_gen = live_gen + 1 where id = p_room;
 perform geobr_private.collab_note(p_room, null, 'resync', '', format('%s recarregou a sala para todos', geobr_private.collab_display_name()));
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.collab_new_code(p_room uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare alphabet text = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'; v_code text; raw bytea; i int;
begin
 perform geobr_private.require_access();
 if geobr_private.collab_mod_check(p_room, null) < 3 then raise exception 'Somente o host troca o codigo'; end if;
 loop
  raw = uuid_send(gen_random_uuid()); v_code = '';
  for i in 0..7 loop v_code = v_code || substr(alphabet, (get_byte(raw, i) % 32) + 1, 1); end loop;
  exit when not exists (select 1 from public.geobr_collab_rooms r where r.code = v_code);
 end loop;
 update public.geobr_collab_rooms set code = v_code, updated_at = now() where id = p_room;
 perform geobr_private.collab_note(p_room, null, 'code', '', format('%s trocou o codigo da sala', geobr_private.collab_display_name()));
 return jsonb_build_object('ok', true, 'code', v_code);
end $$;

create or replace function geobr_private.collab_lock(p_room uuid, p_on boolean)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_mod_check(p_room, null);
 update public.geobr_collab_rooms set locked = coalesce(p_on, false), updated_at = now() where id = p_room;
 perform geobr_private.collab_note(p_room, null, case when p_on then 'lock' else 'unlock' end, '',
  format('%s %s a entrada na sala', geobr_private.collab_display_name(), case when p_on then 'trancou' else 'abriu' end));
 return jsonb_build_object('ok', true);
end $$;

-- What the room panel shows.
create or replace function geobr_private.collab_room_info(p_room uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
declare me public.geobr_collab_members; c public.geobr_collab_rooms; rank int;
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 select * into c from public.geobr_collab_rooms where id = p_room;
 select * into me from public.geobr_collab_members m where m.room_id = p_room and m.user_id = auth.uid() and to_jsonb(m)->>'removed_at' is null;
 rank = case when me.role = 'owner' then 3 when me.can_save then 2 else 1 end;
 return jsonb_build_object('title', c.title, 'code', c.code, 'locked', c.locked, 'rank', rank, 'role', me.role, 'muted', me.muted,
  'members', (select count(*) from public.geobr_collab_members m where m.room_id = p_room and to_jsonb(m)->>'removed_at' is null),
  'objects', (select count(*) from public.geobr_collab_objects where room_id = p_room and data is not null and left(uid, 2) <> '__'),
  'yellow', (select count(*) from public.geobr_collab_objects where room_id = p_room and data is not null and geobr_private.collab_is_yellow(data)),
  'dupes', case when rank < 2 then 0 else (select coalesce(sum(k - 1), 0) from (select count(*) k from public.geobr_collab_objects
     where room_id = p_room and data is not null and left(uid, 2) <> '__' group by data having count(*) > 1) d) end,
  'history_from', (select min(at) from geobr_private.collab_history where room_id = p_room),
  'bans', case when rank < 2 then '[]'::jsonb else coalesce((select jsonb_agg(jsonb_build_object('user_id', b.user_id, 'display_name', b.display_name, 'created_at', b.created_at) order by b.created_at desc)
     from geobr_private.collab_bans b where b.room_id = p_room and b.active), '[]'::jsonb) end,
  'log', case when rank < 2 then '[]'::jsonb else coalesce((select jsonb_agg(x order by x->>'at' desc) from (
     select jsonb_build_object('action', l.action, 'detail', l.detail, 'at', l.at,
      'actor', geobr_private.collab_name_of(l.actor), 'target', case when l.target is null then '' else geobr_private.collab_name_of(l.target) end) x
     from geobr_private.collab_mod_log l where l.room_id = p_room order by l.at desc limit 20) s), '[]'::jsonb) end);
end $$;

do $$
declare f record;
begin
 for f in select * from (values
  ('collab_live2', 'collab_live2', 'p_room uuid, p_since bigint, p_ops jsonb, p_x real, p_y real, p_chat timestamptz', 'p_room, p_since, p_ops, p_x, p_y, p_chat'),
  ('collab_unban', 'collab_unban', 'p_room uuid, p_user uuid', 'p_room, p_user'),
  ('collab_mute', 'collab_mute', 'p_room uuid, p_user uuid, p_on boolean', 'p_room, p_user, p_on'),
  ('collab_set_viewer', 'collab_set_viewer', 'p_room uuid, p_user uuid, p_on boolean', 'p_room, p_user, p_on'),
  ('collab_revert', 'collab_revert', 'p_room uuid, p_user uuid, p_minutes integer', 'p_room, p_user, p_minutes'),
  ('collab_clean', 'collab_clean', 'p_room uuid, p_what text', 'p_room, p_what'),
  ('collab_restore', 'collab_restore', 'p_room uuid, p_objects jsonb, p_label text', 'p_room, p_objects, p_label'),
  ('collab_resync', 'collab_resync', 'p_room uuid', 'p_room'),
  ('collab_new_code', 'collab_new_code', 'p_room uuid', 'p_room'),
  ('collab_lock', 'collab_lock', 'p_room uuid, p_on boolean', 'p_room, p_on'),
  ('collab_room_info', 'collab_room_info', 'p_room uuid', 'p_room')) as t(name, pub, sig, args)
 loop
  execute format('revoke all on function geobr_private.%I(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function geobr_private.%I(%s) to authenticated', f.name, f.sig);
  execute format('create or replace function public.geobr_%s(%s) returns jsonb language sql set search_path = '''' as $f$ select geobr_private.%I(%s) $f$',
   f.pub, f.sig, f.name, f.args);
  execute format('revoke all on function public.geobr_%s(%s) from public, anon', f.pub, f.sig);
  execute format('grant execute on function public.geobr_%s(%s) to authenticated', f.pub, f.sig);
 end loop;
end $$;

insert into geobr_private.releases(version, enabled, message)
values ('1.17.0', true, 'GeoBR 1.17.0: collab profissional (moderacao, reverter, aviso do chat) e fim do portal amarelo infinito')
on conflict (version) do nothing;
