-- GeoBR 1.15.0: online status, social missions, notifications for GD messages,
-- friend requests and collab invites. Non-destructive: new columns, wider
-- checks, new functions and new quest steps appended to the current season.

-- Online status ---------------------------------------------------------------
alter table geobr_gdps.accounts add column if not exists last_seen timestamptz;
alter table geobr_gdps.accounts add column if not exists presence_visibility text not null default 'all';
do $$ begin
 if not exists (select 1 from pg_constraint where conname = 'accounts_presence_visibility_check') then
  alter table geobr_gdps.accounts add constraint accounts_presence_visibility_check check (presence_visibility in ('all', 'friends', 'none'));
 end if;
end $$;

-- Notifications: GD messages / friend requests may come from unlinked accounts.
alter table public.geobr_social_notifications alter column actor drop not null;
alter table public.geobr_social_notifications drop constraint if exists geobr_social_notifications_kind_check;
alter table public.geobr_social_notifications add constraint geobr_social_notifications_kind_check
 check (kind in ('dm', 'reply', 'group', 'collab', 'gd_message', 'friend', 'collab_invite'));

create or replace function geobr_private.notify_account(p_account bigint, p_actor uuid, p_kind text, p_body text)
returns void language sql security definer set search_path = '' as $$
 insert into public.geobr_social_notifications(owner, actor, kind, body)
 select a.supabase_user, p_actor, p_kind, left(p_body, 160) from geobr_gdps.accounts a
 where a.account_id = p_account and a.supabase_user is not null and a.supabase_user is distinct from p_actor
$$;
revoke all on function geobr_private.notify_account(bigint, uuid, text, text) from public, anon, authenticated;

-- Social missions -------------------------------------------------------------
alter table public.geobr_quest_steps drop constraint if exists geobr_quest_steps_kind_check;
alter table public.geobr_quest_steps add constraint geobr_quest_steps_kind_check check (kind in
 ('complete', 'death', 'reach', 'follow_elders', 'comment', 'like', 'rate', 'friend', 'collab_objects', 'gd_message'));

-- Progress for the player's current mission when it is of this kind (each
-- key counts once per mission run). Called by the server, never by clients.
create or replace function geobr_private.social_progress(p_user uuid, p_kind text, p_key text, p_amount int default 1)
returns void language plpgsql security definer set search_path = '' as $$
declare sid bigint; r geobr_private.quest_runs; inserted int; previous text;
begin
 if p_user is null then return; end if;
 select id into sid from public.geobr_pass_seasons where published and starts_at <= now() and ends_at > now() limit 1;
 if sid is null then return; end if;
 select * into r from geobr_private.quest_runs where user_id = p_user and season_id = sid for update;
 if not found or r.definition ->> 'kind' is distinct from p_kind then return; end if;
 insert into geobr_private.quest_distinct(user_id, run_id, match_key) values (p_user, r.run_id, p_kind || ':' || left(coalesce(p_key, ''), 80))
 on conflict do nothing;
 get diagnostics inserted = row_count;
 if inserted = 0 then return; end if;
 update geobr_private.quest_runs set progress = progress + greatest(1, coalesce(p_amount, 1))
  where user_id = p_user and season_id = sid returning * into r;
 if r.progress >= (r.definition ->> 'goal')::int then
  previous = current_setting('request.jwt.claim.sub', true);
  perform set_config('request.jwt.claim.sub', p_user::text, true);
  perform geobr_private.advance_quest(sid, false);
  perform set_config('request.jwt.claim.sub', coalesce(previous, ''), true);
 end if;
end $$;
revoke all on function geobr_private.social_progress(uuid, text, text, int) from public, anon, authenticated;

-- Elder Moderators (Owner role or a role with mod.elder), without yourself.
create or replace function geobr_private.elders()
returns jsonb language sql stable security definer set search_path = '' as $$
 select coalesce(jsonb_agg(jsonb_build_object('account_id', x.account_id, 'username', x.username) order by x.username), '[]'::jsonb)
 from (select distinct a.account_id::int, a.username from geobr_gdps.account_roles ar
  join geobr_gdps.roles r on r.role_id = ar.role_id join geobr_gdps.accounts a on a.account_id = ar.account_id
  where (r.is_owner or r.permissions @> array['mod.elder']) and not a.is_banned and a.supabase_user is distinct from auth.uid()) x
$$;

-- The mod checks which Elder Moderators this player follows in GD (follows
-- are kept by the game) and sends them; all followed completes the mission.
create or replace function geobr_private.quest_follow_elders(p_followed bigint[])
returns jsonb language plpgsql security definer set search_path = '' as $$
declare need int; have int;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 select count(*), count(*) filter (where (e ->> 'account_id')::bigint = any(coalesce(p_followed, '{}')))
  into need, have from jsonb_array_elements(geobr_private.elders()) e;
 if need > 0 and have >= need then perform geobr_private.social_progress(auth.uid(), 'follow_elders', 'all', 1); end if;
 return jsonb_build_object('elders', need, 'followed', have, 'state', geobr_private.state_v2());
end $$;

-- Collab invite: the host / ADMs call a player (GDPS username, linked to GeoBR).
create or replace function geobr_private.collab_invite(p_room uuid, p_username text)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare c public.geobr_collab_rooms; target geobr_gdps.accounts; sent int;
begin
 perform geobr_private.require_access();
 perform geobr_private.collab_role_or_fail(p_room);
 if not geobr_private.collab_can_save(p_room) then raise exception 'Somente o host e os ADMs convidam'; end if;
 select * into c from public.geobr_collab_rooms where id = p_room;
 select * into target from geobr_gdps.accounts where lower(username) = lower(trim(coalesce(p_username, ''))) limit 1;
 if target.account_id is null then raise exception 'Jogador nao encontrado no GDPS'; end if;
 if target.supabase_user is null then raise exception 'Esse jogador ainda nao vinculou a conta GeoBR'; end if;
 select count(*) into sent from public.geobr_social_notifications where actor = auth.uid() and kind = 'collab_invite' and created_at > now() - interval '1 hour';
 if sent >= 20 then raise exception 'Muitos convites; tente mais tarde'; end if;
 perform geobr_private.notify_account(target.account_id, auth.uid(), 'collab_invite',
  format('%s te chamou para o collab "%s" | Codigo: %s', geobr_private.collab_display_name(), c.title, c.code));
 return jsonb_build_object('ok', true, 'username', target.username);
end $$;

-- Collab: new objects count for the "collab_objects" mission.
create or replace function geobr_private.collab_live(p_room uuid, p_since bigint, p_ops jsonb, p_x real, p_y real)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare r text; u uuid = auth.uid(); n int; s bigint; e jsonb; upto bigint; ops jsonb; saver boolean; created int = 0; was boolean;
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
   was = exists (select 1 from public.geobr_collab_objects where room_id = p_room and uid = e->>'u' and data is not null);
   insert into public.geobr_collab_objects(room_id, uid, data, seq, updated_by) values (p_room, e->>'u', e->>'d', s, u)
    on conflict (room_id, uid) do update set data = excluded.data, seq = excluded.seq, updated_by = excluded.updated_by;
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

do $$
declare f record;
begin
 for f in select * from (values
  ('elders', ''), ('quest_follow_elders', 'p_followed bigint[]'), ('collab_invite', 'p_room uuid, p_username text')) as t(name, sig)
 loop
  execute format('revoke all on function geobr_private.%I(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function geobr_private.%I(%s) to authenticated', f.name, f.sig);
  execute format('create or replace function public.geobr_%s(%s) returns jsonb language sql set search_path = '''' as $f$ select geobr_private.%I(%s) $f$',
   f.name, f.sig, f.name, (select string_agg(split_part(trim(x), ' ', 1), ', ') from unnest(string_to_array(nullif(f.sig, ''), ',')) x));
  execute format('revoke all on function public.geobr_%s(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function public.geobr_%s(%s) to authenticated', f.name, f.sig);
 end loop;
end $$;

-- New missions at the end of the current season's sequence.
do $$
declare sid bigint; base int;
begin
 for sid in select id from public.geobr_pass_seasons where published and ends_at > now() loop
  if exists (select 1 from public.geobr_quest_steps where season_id = sid and kind = 'follow_elders') then continue; end if;
  select coalesce(max(sequence_order), 0) into base from public.geobr_quest_steps where season_id = sid;
  insert into public.geobr_quest_steps(season_id, sequence_order, title, kind, goal, xp, tokens) values
   (sid, base + 1, 'Siga todos os Elder Moderators do GDPS', 'follow_elders', 1, 150, 10),
   (sid, base + 2, 'Coloque 50 objetos numa sala de collab', 'collab_objects', 50, 220, 14),
   (sid, base + 3, 'Comente em 2 fases diferentes', 'comment', 2, 120, 8),
   (sid, base + 4, 'Faca um novo amigo no GDPS', 'friend', 1, 160, 10),
   (sid, base + 5, 'De like em 3 fases', 'like', 3, 100, 6),
   (sid, base + 6, 'Mande uma mensagem para alguem', 'gd_message', 1, 90, 6),
   (sid, base + 7, 'Vote na dificuldade de 2 fases', 'rate', 2, 100, 6);
 end loop;
end $$;

insert into geobr_private.releases(version, enabled, message)
values ('1.15.0', true, 'GeoBR 1.15.0: status online, missoes sociais, notificacoes, collab completo')
on conflict (version) do nothing;
