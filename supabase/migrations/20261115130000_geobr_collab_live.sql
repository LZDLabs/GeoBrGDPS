-- GeoBR 1.12.0: live collab editor.
-- The room's level is kept object by object (uid -> GD object string) with a
-- room-wide sequence, so every editor sends its changes and receives the
-- others' about once per second. Presence keeps each member's name and the
-- point of the level they are looking at. Saving the room's level (snapshot)
-- and publishing is limited to the owner and the members the owner marks as
-- admins (can_save). Non-destructive: new columns, tables and functions only.

alter table public.geobr_collab_rooms add column if not exists live_seq bigint not null default 0;
alter table public.geobr_collab_members add column if not exists can_save boolean not null default false;

create table if not exists public.geobr_collab_objects (
    room_id uuid not null references public.geobr_collab_rooms(id) on delete cascade,
    uid text not null check (uid ~ '^[A-Za-z0-9_.-]{1,40}$'),
    data text check (data is null or octet_length(data) <= 16384),
    seq bigint not null,
    updated_by uuid references auth.users(id) on delete set null,
    primary key (room_id, uid)
);
create index if not exists geobr_collab_objects_seq_idx on public.geobr_collab_objects(room_id, seq);

create table if not exists public.geobr_collab_presence (
    room_id uuid not null references public.geobr_collab_rooms(id) on delete cascade,
    user_id uuid not null references auth.users(id) on delete cascade,
    display_name text not null check (char_length(display_name) between 1 and 32),
    x real not null default 0,
    y real not null default 0,
    updated_at timestamptz not null default now(),
    primary key (room_id, user_id)
);

alter table public.geobr_collab_objects enable row level security;
alter table public.geobr_collab_presence enable row level security;
revoke all on public.geobr_collab_objects from anon, authenticated;
revoke all on public.geobr_collab_presence from anon, authenticated;

create or replace function geobr_private.collab_can_save(p_room uuid)
returns boolean language sql stable security definer set search_path = '' as $$
 select exists (select 1 from public.geobr_collab_members m join public.geobr_collab_rooms c on c.id = m.room_id
  where m.room_id = p_room and m.user_id = auth.uid() and c.active and (m.role = 'owner' or m.can_save))
$$;
revoke all on function geobr_private.collab_can_save(uuid) from public, anon, authenticated;

create or replace function geobr_private.collab_room_json(p_room uuid)
returns jsonb language sql stable security definer set search_path = '' as $$
 select jsonb_build_object('id', c.id, 'code', c.code, 'title', c.title, 'owner_id', c.owner_id,
  'role', (select role from public.geobr_collab_members where room_id = c.id and user_id = auth.uid()),
  'can_save', geobr_private.collab_can_save(c.id),
  'revision', coalesce(s.revision, 0), 'updated_at', coalesce(s.updated_at, c.updated_at),
  'members', (select count(*) from public.geobr_collab_members where room_id = c.id),
  'gd_level_id', c.gd_level_id)
 from public.geobr_collab_rooms c left join public.geobr_collab_snapshots s on s.room_id = c.id where c.id = p_room
$$;
revoke all on function geobr_private.collab_room_json(uuid) from public, anon, authenticated;

-- Saving the room's level: owner and admins only. p_base < 0 overwrites
-- (the live objects are the source of truth; the snapshot is their save).
create or replace function geobr_private.collab_push(p_room uuid, p_level text, p_base bigint)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare s public.geobr_collab_snapshots;
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 if not geobr_private.collab_can_save(p_room) then raise exception 'Somente o host e os ADMs da sala salvam o level'; end if;
 if p_level is null or octet_length(p_level) > 8388608 then raise exception 'Level grande demais (8 MiB)'; end if;
 select * into s from public.geobr_collab_snapshots where room_id = p_room for update;
 if not found then
  insert into public.geobr_collab_snapshots(room_id, revision, level_string, updated_by) values (p_room, 0, '', auth.uid()) returning * into s;
 end if;
 if coalesce(p_base, -1) >= 0 and s.revision <> p_base then
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
  || jsonb_build_object('role', m.role, 'can_save', m.role = 'owner' or m.can_save) order by m.role <> 'owner', m.joined_at)
  from public.geobr_collab_members m where m.room_id = p_room), '[]'::jsonb);
end $$;

-- The owner marks/unmarks a member as admin (can save and publish).
create or replace function geobr_private.collab_set_saver(p_room uuid, p_user uuid, p_on boolean)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 if geobr_private.collab_role_or_fail(p_room) <> 'owner' then raise exception 'Somente o host define os ADMs'; end if;
 update public.geobr_collab_members set can_save = coalesce(p_on, false)
  where room_id = p_room and user_id = p_user and role <> 'owner';
 if not found then raise exception 'Membro nao encontrado'; end if;
 return jsonb_build_object('ok', true);
end $$;

-- Every live object of the room (the editor rebuilds the level from it).
create or replace function geobr_private.collab_live_load(p_room uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 return jsonb_build_object(
  'seq', (select live_seq from public.geobr_collab_rooms where id = p_room),
  'can_save', geobr_private.collab_can_save(p_room),
  'objects', coalesce((select jsonb_agg(jsonb_build_object('u', o.uid, 'd', o.data))
    from public.geobr_collab_objects o where o.room_id = p_room and o.data is not null), '[]'::jsonb));
end $$;

-- First editor to open a room without live objects seeds it from its level.
create or replace function geobr_private.collab_live_seed(p_room uuid, p_objects jsonb)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare r text; c public.geobr_collab_rooms; n int;
begin
 perform geobr_private.require_access();
 r = geobr_private.collab_role_or_fail(p_room);
 if r not in ('owner', 'editor') then raise exception 'Somente leitura nesta sala'; end if;
 if jsonb_typeof(p_objects) <> 'array' or jsonb_array_length(p_objects) > 100000 then raise exception 'Level invalido'; end if;
 select * into c from public.geobr_collab_rooms where id = p_room for update;
 if c.live_seq <> 0 or exists (select 1 from public.geobr_collab_objects where room_id = p_room) then
  return jsonb_build_object('ok', false);
 end if;
 n = jsonb_array_length(p_objects);
 insert into public.geobr_collab_objects(room_id, uid, data, seq, updated_by)
  select p_room, e->>'u', e->>'d', 1, auth.uid() from jsonb_array_elements(p_objects) e
  where e->>'d' is not null on conflict do nothing;
 update public.geobr_collab_rooms set live_seq = 1, updated_at = now() where id = p_room;
 return jsonb_build_object('ok', true, 'seq', 1, 'objects', n);
end $$;

-- One live round: applies this editor's changes (ops: [{u, d}] with d null =
-- deleted), stores its view position and returns the changes after p_since
-- plus who else is in the editor right now.
create or replace function geobr_private.collab_live(p_room uuid, p_since bigint, p_ops jsonb, p_x real, p_y real)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare r text; u uuid = auth.uid(); n int; s bigint; e jsonb; upto bigint; ops jsonb;
begin
 perform geobr_private.require_access();
 r = geobr_private.collab_role_or_fail(p_room);
 if p_ops is null or jsonb_typeof(p_ops) <> 'array' then p_ops = '[]'::jsonb; end if;
 n = jsonb_array_length(p_ops);
 if n > 0 then
  if r not in ('owner', 'editor') then raise exception 'Somente leitura nesta sala'; end if;
  if n > 2000 then raise exception 'Muitas alteracoes de uma vez'; end if;
  update public.geobr_collab_rooms set live_seq = live_seq + n, updated_at = now() where id = p_room returning live_seq - n into s;
  for e in select * from jsonb_array_elements(p_ops) loop
   s = s + 1;
   if coalesce(e->>'u', '') !~ '^[A-Za-z0-9_.-]{1,40}$' or octet_length(coalesce(e->>'d', '')) > 16384 then continue; end if;
   insert into public.geobr_collab_objects(room_id, uid, data, seq, updated_by) values (p_room, e->>'u', e->>'d', s, u)
    on conflict (room_id, uid) do update set data = excluded.data, seq = excluded.seq, updated_by = excluded.updated_by;
  end loop;
 end if;
 insert into public.geobr_collab_presence(room_id, user_id, display_name, x, y, updated_at)
  values (p_room, u, geobr_private.collab_display_name(), coalesce(p_x, 0), coalesce(p_y, 0), now())
  on conflict (room_id, user_id) do update set display_name = excluded.display_name, x = excluded.x, y = excluded.y, updated_at = now();
 select coalesce(jsonb_agg(jsonb_build_object('u', o.uid, 'd', o.data, 'mine', o.updated_by = u) order by o.seq), '[]'::jsonb), max(o.seq)
  into ops, upto
  from (select * from public.geobr_collab_objects where room_id = p_room and seq > coalesce(p_since, 0) order by seq limit 3000) o;
 return jsonb_build_object(
  'seq', coalesce(upto, greatest(coalesce(p_since, 0), (select live_seq from public.geobr_collab_rooms where id = p_room))),
  'more', upto is not null and upto < (select live_seq from public.geobr_collab_rooms where id = p_room),
  'ops', ops,
  'presence', coalesce((select jsonb_agg(jsonb_build_object('name', p.display_name, 'x', p.x, 'y', p.y))
    from public.geobr_collab_presence p where p.room_id = p_room and p.user_id <> u and p.updated_at > now() - interval '12 seconds'), '[]'::jsonb));
end $$;

do $$
declare f record;
begin
 for f in select * from (values
  ('collab_set_saver', 'collab_set_saver', 'p_room uuid, p_user uuid, p_on boolean', 'p_room, p_user, p_on'),
  ('collab_live_load', 'collab_live_load', 'p_room uuid', 'p_room'),
  ('collab_live_seed', 'collab_live_seed', 'p_room uuid, p_objects jsonb', 'p_room, p_objects'),
  ('collab_live', 'collab_live', 'p_room uuid, p_since bigint, p_ops jsonb, p_x real, p_y real', 'p_room, p_since, p_ops, p_x, p_y')) as t(name, pub, sig, args)
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
values ('1.12.0', true, 'GeoBR 1.12.0: collab ao vivo')
on conflict (version) do nothing;
