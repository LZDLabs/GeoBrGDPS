-- GeoBR 1.14.0: the collab room carries the level's music settings as the
-- special live item "__settings" (written only by the host and the ADMs).
-- 1.12/1.13 clients would try to build an object from it, so it is never sent
-- to them (x-geobr-version header). Non-destructive: function replacements.

create or replace function geobr_private.collab_client_has_settings()
returns boolean language sql stable set search_path = '' as $$
 select coalesce((current_setting('request.headers', true)::json ->> 'x-geobr-version') not in ('1.12.0', '1.13.0'), false)
$$;
revoke all on function geobr_private.collab_client_has_settings() from public, anon, authenticated;

create or replace function geobr_private.collab_live_load(p_room uuid)
returns jsonb language plpgsql stable security definer set search_path = '' as $$
declare settings boolean = geobr_private.collab_client_has_settings();
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 return jsonb_build_object(
  'seq', (select live_seq from public.geobr_collab_rooms where id = p_room),
  'can_save', geobr_private.collab_can_save(p_room),
  'objects', coalesce((select jsonb_agg(jsonb_build_object('u', o.uid, 'd', o.data))
    from public.geobr_collab_objects o where o.room_id = p_room and o.data is not null and (settings or left(o.uid, 2) <> '__')), '[]'::jsonb));
end $$;

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
  where e->>'d' is not null and (saver or left(e->>'u', 2) <> '__') on conflict do nothing;
 update public.geobr_collab_rooms set live_seq = 1, updated_at = now() where id = p_room;
 return jsonb_build_object('ok', true, 'seq', 1, 'objects', n);
end $$;

create or replace function geobr_private.collab_live(p_room uuid, p_since bigint, p_ops jsonb, p_x real, p_y real)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare r text; u uuid = auth.uid(); n int; s bigint; e jsonb; upto bigint; ops jsonb; saver boolean;
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
   -- Level settings (music): host and ADMs only.
   if left(e->>'u', 2) = '__' and not saver then continue; end if;
   insert into public.geobr_collab_objects(room_id, uid, data, seq, updated_by) values (p_room, e->>'u', e->>'d', s, u)
    on conflict (room_id, uid) do update set data = excluded.data, seq = excluded.seq, updated_by = excluded.updated_by;
  end loop;
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

insert into geobr_private.releases(version, enabled, message)
values ('1.14.0', true, 'GeoBR 1.14.0: musica no collab ao vivo, editor sem crash')
on conflict (version) do nothing;

-- 1.14.1: collab save without NUL bytes in the level.
insert into geobr_private.releases(version, enabled, message)
values ('1.14.1', true, 'GeoBR 1.14.1: salvar collab sem erro de escape')
on conflict (version) do nothing;

-- 1.14.2: safer live collab.
insert into geobr_private.releases(version, enabled, message)
values ('1.14.2', true, 'GeoBR 1.14.2: collab ao vivo mais seguro')
on conflict (version) do nothing;
