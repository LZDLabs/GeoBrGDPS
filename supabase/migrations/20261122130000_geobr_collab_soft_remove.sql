-- GeoBR 1.17.0: collab members are never deleted any more. Kick, ban and
-- leaving mark the membership (removed_at); every membership check only sees
-- active members, and joining again (when not banned) reactivates the row.
-- Non-destructive: one new column and function replacements (same signatures).

alter table public.geobr_collab_members add column if not exists removed_at timestamptz;
create index if not exists geobr_collab_members_active_idx on public.geobr_collab_members(room_id) where removed_at is null;

create or replace function geobr_private.collab_role_or_fail(p_room uuid)
returns text language plpgsql stable security definer set search_path = '' as $$
declare r text;
begin
 select m.role into r from public.geobr_collab_members m join public.geobr_collab_rooms c on c.id = m.room_id
  where m.room_id = p_room and m.user_id = auth.uid() and m.removed_at is null and c.active;
 if r is null then raise exception 'Voce nao esta nesta sala'; end if;
 return r;
end $$;

create or replace function geobr_private.collab_can_save(p_room uuid)
returns boolean language sql stable security definer set search_path = '' as $$
 select exists (select 1 from public.geobr_collab_members m join public.geobr_collab_rooms c on c.id = m.room_id
  where m.room_id = p_room and m.user_id = auth.uid() and m.removed_at is null and c.active and (m.role = 'owner' or m.can_save))
$$;

create or replace function geobr_private.is_collab_member(target_room uuid)
returns boolean language sql stable security definer set search_path = '' as $$
    select (select auth.uid()) is not null and exists (
        select 1
        from public.geobr_collab_members member
        where member.room_id = target_room
          and member.user_id = (select auth.uid())
          and member.removed_at is null
    );
$$;

create or replace function geobr_private.can_edit_collab(target_room uuid)
returns boolean language sql stable security definer set search_path = '' as $$
    select (select auth.uid()) is not null and exists (
        select 1
        from public.geobr_collab_members member
        where member.room_id = target_room
          and member.user_id = (select auth.uid())
          and member.removed_at is null
          and member.role in ('owner', 'editor')
    );
$$;

create or replace function geobr_private.collab_room_json(p_room uuid)
returns jsonb language sql stable security definer set search_path = '' as $$
 select jsonb_build_object('id', c.id, 'code', c.code, 'title', c.title, 'owner_id', c.owner_id,
  'role', (select role from public.geobr_collab_members where room_id = c.id and user_id = auth.uid() and removed_at is null),
  'can_save', geobr_private.collab_can_save(c.id),
  'revision', coalesce(s.revision, 0), 'updated_at', coalesce(s.updated_at, c.updated_at),
  'members', (select count(*) from public.geobr_collab_members where room_id = c.id and removed_at is null),
  'gd_level_id', c.gd_level_id)
 from public.geobr_collab_rooms c left join public.geobr_collab_snapshots s on s.room_id = c.id where c.id = p_room
$$;

create or replace function geobr_private.collab_rooms()
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 return coalesce((select jsonb_agg(geobr_private.collab_room_json(c.id) order by c.updated_at desc)
  from public.geobr_collab_rooms c join public.geobr_collab_members m on m.room_id = c.id
  where m.user_id = auth.uid() and m.removed_at is null and c.active), '[]'::jsonb);
end $$;

create or replace function geobr_private.collab_member_list(p_room uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 return coalesce((select jsonb_agg(coalesce(geobr_private.player_json(m.user_id), jsonb_build_object('user_id', m.user_id, 'display_name', m.display_name))
  || jsonb_build_object('role', m.role, 'can_save', m.role = 'owner' or m.can_save, 'muted', m.muted) order by m.role <> 'owner', m.joined_at)
  from public.geobr_collab_members m where m.room_id = p_room and m.removed_at is null), '[]'::jsonb);
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
   from public.geobr_collab_members m where m.room_id = room.id and m.removed_at is null), '[]'::jsonb));
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
  from public.geobr_collab_members m where m.room_id = p_room and m.user_id <> u and m.removed_at is null;
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.collab_set_saver(p_room uuid, p_user uuid, p_on boolean)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 if geobr_private.collab_role_or_fail(p_room) <> 'owner' then raise exception 'Somente o host define os ADMs'; end if;
 update public.geobr_collab_members set can_save = coalesce(p_on, false)
  where room_id = p_room and user_id = p_user and role <> 'owner' and removed_at is null;
 if not found then raise exception 'Membro nao encontrado'; end if;
 return jsonb_build_object('ok', true);
end $$;

create or replace function geobr_private.collab_mod_check(p_room uuid, p_target uuid, p_allow_gone boolean default false)
returns integer language plpgsql stable security definer set search_path = '' as $$
declare me public.geobr_collab_members; t public.geobr_collab_members; mine int; theirs int;
begin
 perform geobr_private.collab_role_or_fail(p_room);
 select * into me from public.geobr_collab_members where room_id = p_room and user_id = auth.uid() and removed_at is null;
 mine = case when me.role = 'owner' then 3 when me.can_save then 2 else 1 end;
 if mine < 2 then raise exception 'Somente o host e os ADMs moderam a sala'; end if;
 if p_target is null then return mine; end if;
 if p_target = auth.uid() then raise exception 'Voce nao pode fazer isso com voce mesmo'; end if;
 select * into t from public.geobr_collab_members where room_id = p_room and user_id = p_target and removed_at is null;
 if t.user_id is null then
  if p_allow_gone then return mine; end if;
  raise exception 'Membro nao encontrado';
 end if;
 theirs = case when t.role = 'owner' then 3 when t.can_save then 2 else 1 end;
 if theirs >= mine then raise exception 'Voce nao pode moderar esse membro'; end if;
 return mine;
end $$;

create or replace function geobr_private.collab_live2(p_room uuid, p_since bigint, p_ops jsonb, p_x real, p_y real, p_chat timestamptz)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); c public.geobr_collab_rooms; m public.geobr_collab_members; why text; res jsonb;
begin
 perform geobr_private.require_access();
 select * into c from public.geobr_collab_rooms where id = p_room;
 select * into m from public.geobr_collab_members where room_id = p_room and user_id = u and removed_at is null;
 if c.id is null or not c.active or m.user_id is null then
  if c.id is null or not c.active then why = 'closed';
  elsif exists (select 1 from geobr_private.collab_bans where room_id = p_room and user_id = u and active) then why = 'ban';
  else
   select l.action into why from geobr_private.collab_mod_log l where l.room_id = p_room and l.target = u and l.action in ('kick', 'ban', 'leave')
    order by l.at desc limit 1;
   why = case when why = 'kick' then 'kick' when why = 'ban' then 'ban' else 'left' end;
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

-- Joining: a removed member comes back on the same row (unless banned).
create or replace function geobr_private.collab_join(p_code text)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare u uuid = auth.uid(); room public.geobr_collab_rooms; mem public.geobr_collab_members;
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
 select * into mem from public.geobr_collab_members where room_id = room.id and user_id = u;
 if mem.user_id is null or mem.removed_at is not null then
  if exists (select 1 from geobr_private.collab_bans where room_id = room.id and user_id = u and active) then
   return jsonb_build_object('ok', false, 'error', 'Voce foi banido desta sala.');
  end if;
  if room.locked then
   return jsonb_build_object('ok', false, 'error', 'A sala esta trancada pelo host.');
  end if;
  if (select count(*) from public.geobr_collab_members where room_id = room.id and removed_at is null) >= 8 then
   return jsonb_build_object('ok', false, 'error', 'Sala cheia (8 pessoas).');
  end if;
  if (select count(*) from public.geobr_collab_members where user_id = u and removed_at is null) >= 20 then
   return jsonb_build_object('ok', false, 'error', 'Voce ja esta em 20 salas.');
  end if;
  if mem.user_id is null then
   insert into public.geobr_collab_members(room_id, user_id, role, display_name) values (room.id, u, 'editor', geobr_private.collab_display_name());
  else
   update public.geobr_collab_members set removed_at = null, role = 'editor', can_save = false, muted = false, joined_at = now(),
    display_name = geobr_private.collab_display_name() where room_id = room.id and user_id = u;
  end if;
  insert into public.geobr_social_notifications(owner, actor, kind, body)
   select m.user_id, u, 'collab', left(geobr_private.collab_display_name() || ' entrou no collab ' || room.title, 200)
   from public.geobr_collab_members m where m.room_id = room.id and m.user_id <> u and m.removed_at is null;
 end if;
 insert into geobr_private.collab_join_attempts(user_id, ok) values (u, true);
 return jsonb_build_object('ok', true, 'room', geobr_private.collab_room_json(room.id));
end $$;

-- Leaving marks the row; the host's place goes to the oldest active member.
create or replace function geobr_private.collab_leave(p_room uuid)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare r text; heir uuid;
begin
 perform geobr_private.require_access();
 r = geobr_private.collab_role_or_fail(p_room);
 update public.geobr_collab_members set removed_at = now(), can_save = false, muted = false,
  role = case when role = 'owner' then 'editor' else role end
  where room_id = p_room and user_id = auth.uid();
 insert into geobr_private.collab_mod_log(room_id, actor, target, action) values (p_room, auth.uid(), auth.uid(), 'leave');
 if r = 'owner' then
  select user_id into heir from public.geobr_collab_members where room_id = p_room and removed_at is null order by joined_at limit 1;
  if heir is null then update public.geobr_collab_rooms set active = false, updated_at = now() where id = p_room;
  else
   update public.geobr_collab_members set role = 'owner' where room_id = p_room and user_id = heir;
   update public.geobr_collab_rooms set owner_id = heir, updated_at = now() where id = p_room;
  end if;
 end if;
 return jsonb_build_object('ok', true);
end $$;

-- Kick (can come back with the code) or ban (cannot).
create or replace function geobr_private.collab_kick(p_room uuid, p_user uuid, p_ban boolean)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare target text; title text;
begin
 perform geobr_private.require_access();
 -- A member already kicked can still be banned.
 perform geobr_private.collab_mod_check(p_room, p_user, coalesce(p_ban, false));
 target = geobr_private.collab_name_of(p_user);
 select c.title into title from public.geobr_collab_rooms c where c.id = p_room;
 update public.geobr_collab_members set removed_at = now(), can_save = false, muted = false
  where room_id = p_room and user_id = p_user and role <> 'owner';
 update public.geobr_collab_presence set updated_at = now() - interval '1 hour' where room_id = p_room and user_id = p_user;
 if coalesce(p_ban, false) then
  insert into geobr_private.collab_bans(room_id, user_id, display_name, banned_by) values (p_room, p_user, target, auth.uid())
  on conflict (room_id, user_id) do update set active = true, display_name = excluded.display_name, banned_by = excluded.banned_by, created_at = now();
 end if;
 perform geobr_private.collab_note(p_room, p_user, case when p_ban then 'ban' else 'kick' end, '',
  format('%s %s %s da sala', geobr_private.collab_display_name(), case when p_ban then 'baniu' else 'expulsou' end, target));
 insert into public.geobr_social_notifications(owner, actor, kind, body)
  values (p_user, auth.uid(), 'collab', left(format('Voce foi %s do collab %s', case when p_ban then 'banido' else 'removido' end, title), 200));
 return jsonb_build_object('ok', true);
end $$;

do $$
begin
 revoke all on function geobr_private.collab_kick(uuid, uuid, boolean) from public, anon;
 grant execute on function geobr_private.collab_kick(uuid, uuid, boolean) to authenticated;
 create or replace function public.geobr_collab_kick(p_room uuid, p_user uuid, p_ban boolean) returns jsonb language sql set search_path = ''
  as $f$ select geobr_private.collab_kick(p_room, p_user, p_ban) $f$;
 revoke all on function public.geobr_collab_kick(uuid, uuid, boolean) from public, anon;
 grant execute on function public.geobr_collab_kick(uuid, uuid, boolean) to authenticated;
end $$;
