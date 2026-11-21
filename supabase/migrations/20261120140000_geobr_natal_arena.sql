-- GeoBR Natal 2026, part 2. Staff test mode (the event works for the staff
-- before it opens, with a simulated December day), the "Batalha Natalina"
-- (the owner opens it, everyone presses Entrar during a 3 minute lobby, the
-- map is voted, bots for testing), the event level that changes on a timer
-- (with a countdown to the next one) and an owner overview. Non-destructive:
-- players who leave are marked inactive, nothing is deleted.

alter table geobr_private.natal_config add column if not exists test_mode boolean not null default true;
alter table geobr_private.natal_config add column if not exists test_day int not null default 0;
alter table geobr_private.natal_config add column if not exists lobby_seconds int not null default 180;
alter table geobr_private.natal_config add column if not exists rotation_hours int not null default 24;

-- Staff = GeoBR admin session, or a GDPS role that manages events (owner/admin).
create or replace function geobr_private.natal_staff() returns boolean language sql stable security definer set search_path = '' as $$
 select auth.uid() is not null and (geobr_private.is_admin() or exists (
  select 1 from geobr_gdps.accounts a join geobr_gdps.account_roles ar on ar.account_id = a.account_id
  join geobr_gdps.roles r on r.role_id = ar.role_id
  where a.supabase_user = auth.uid() and (r.is_owner or r.permissions && array['*', 'events.manage'])))
$$;

create or replace function geobr_private.natal_active() returns boolean language sql stable security definer set search_path = '' as $$
 select coalesce((select (enabled and now() >= starts_at and now() < ends_at) or (test_mode and geobr_private.natal_staff())
  from geobr_private.natal_config where id = 1), false) $$;

create or replace function geobr_private.natal_today() returns date language sql stable security definer set search_path = '' as $$
 select case when c.test_mode and c.test_day between 1 and 25 and geobr_private.natal_staff()
  then make_date(extract(year from c.starts_at at time zone 'America/Sao_Paulo')::int, 12, c.test_day)
  else (now() at time zone 'America/Sao_Paulo')::date end
 from geobr_private.natal_config c where c.id = 1 $$;

-- Batalha Natalina ---------------------------------------------------------------
create table if not exists geobr_private.natal_arena (
 id bigint generated always as identity primary key,
 state text not null default 'lobby' check (state in ('lobby','running','finished','cancelled')),
 opened_by uuid,
 lobby_until timestamptz not null,
 forced_level bigint,
 level_id bigint references public.geobr_natal_levels(id) on delete set null,
 mode text check (mode in ('classic','platformer')),
 starts_at timestamptz, ends_at timestamptz,
 bots int not null default 0 check (bots between 0 and 8),
 created_at timestamptz not null default now()
);
create table if not exists geobr_private.natal_arena_players (
 arena_id bigint not null references geobr_private.natal_arena(id) on delete cascade,
 slot text not null,
 user_id uuid references auth.users(id) on delete cascade,
 bot boolean not null default false,
 display_name text not null,
 accessory text,
 vote bigint,
 percent int not null default 0 check (percent between 0 and 100),
 best_ms int,
 finished boolean not null default false,
 guard_mods text[] not null default '{}',
 disqualified boolean not null default false,
 bot_target int, bot_pace real,
 place int, prize int not null default 0,
 active boolean not null default true,
 joined_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 primary key (arena_id, slot)
);
alter table geobr_private.natal_arena_players add column if not exists active boolean not null default true;
do $$ declare t text; begin
 foreach t in array array['natal_arena','natal_arena_players'] loop
  execute format('alter table geobr_private.%I enable row level security', t);
  execute format('revoke all on geobr_private.%I from public, anon, authenticated', t);
 end loop;
end $$;

-- The battle on screen: an open one, or the last one for 15 minutes (results).
create or replace function geobr_private.natal_arena_current() returns bigint language sql stable security definer set search_path = '' as $$
 select id from geobr_private.natal_arena
 where state in ('lobby','running') or coalesce(ends_at, lobby_until) > now() - interval '15 minutes'
 order by id desc limit 1 $$;

create or replace function geobr_private.natal_arena_add_bots(p_arena bigint, p_count int) returns void language plpgsql security definer set search_path = '' as $$
declare names text[] = array['Bot Rena','Bot Elfo','Bot Noel','Bot Floco','Bot Biscoito','Bot Sino','Bot Estrela','Bot Presente']; i int;
begin
 update geobr_private.natal_arena_players set active = false where arena_id = p_arena and bot and slot > 'bot:' || greatest(0, least(8, p_count));
 for i in 1 .. greatest(0, least(8, p_count)) loop
  insert into geobr_private.natal_arena_players(arena_id, slot, bot, display_name, accessory)
  values (p_arena, 'bot:' || i, true, names[i], (select id from public.geobr_natal_accessories where published order by random() limit 1))
  on conflict (arena_id, slot) do update set active = true;
 end loop;
 update geobr_private.natal_arena set bots = greatest(0, least(8, p_count)) where id = p_arena;
end $$;

-- Moves the battle along: lobby -> map vote -> running -> results. Bots play by
-- the clock (each one has a target % and a pace), so no server timer is needed.
create or replace function geobr_private.natal_arena_tick(p_arena bigint) returns void language plpgsql security definer set search_path = '' as $$
declare a geobr_private.natal_arena; v_level bigint; v_mode text; v_secs int; total real; elapsed real; prizes jsonb; r record; v_place int = 0; v_prize int;
begin
 select * into a from geobr_private.natal_arena where id = p_arena for update;
 if a.id is null then return; end if;
 if a.state = 'lobby' and now() >= a.lobby_until then
  if not exists (select 1 from geobr_private.natal_arena_players where arena_id = a.id and not bot and active) then
   update geobr_private.natal_arena set state = 'cancelled', ends_at = now() where id = a.id; return;
  end if;
  select id into v_level from public.geobr_natal_levels where id = a.forced_level and published;
  if v_level is null then
   select p.vote into v_level from geobr_private.natal_arena_players p join public.geobr_natal_levels l on l.id = p.vote and l.published
   where p.arena_id = a.id and p.active group by p.vote order by count(*) desc, random() limit 1;
  end if;
  if v_level is null then select id into v_level from public.geobr_natal_levels where published order by random() limit 1; end if;
  if v_level is null then update geobr_private.natal_arena set state = 'cancelled', ends_at = now() where id = a.id; return; end if;
  select case when platformer then 'platformer' else 'classic' end into v_mode from public.geobr_natal_levels where id = v_level;
  v_secs = coalesce(((select battle_seconds from geobr_private.natal_config where id = 1) ->> v_mode)::int, 180);
  update geobr_private.natal_arena_players set bot_target = case when random() < 0.5 then 100 else 35 + floor(random() * 60)::int end,
   bot_pace = 0.3 + random() * 0.6 where arena_id = a.id and bot and active;
  update geobr_private.natal_arena set state = 'running', level_id = v_level, mode = v_mode,
   starts_at = now() + interval '10 seconds', ends_at = now() + interval '10 seconds' + make_interval(secs => v_secs) where id = a.id;
  return;
 end if;
 if a.state <> 'running' then return; end if;
 total = greatest(1, extract(epoch from a.ends_at - a.starts_at));
 elapsed = greatest(0, extract(epoch from least(now(), a.ends_at) - a.starts_at));
 update geobr_private.natal_arena_players set
  percent = least(bot_target, floor(bot_target * least(1, elapsed / (bot_pace * total))))::int,
  finished = bot_target = 100 and elapsed >= bot_pace * total,
  best_ms = case when bot_target = 100 and elapsed >= bot_pace * total then round(bot_pace * total * 1000)::int end,
  updated_at = case when bot_target = 100 and elapsed >= bot_pace * total then a.starts_at + make_interval(secs => bot_pace * total) else now() end
 where arena_id = a.id and bot and active and not finished;
 if now() < a.ends_at and exists (select 1 from geobr_private.natal_arena_players where arena_id = a.id and not bot and active and not finished and not disqualified) then return; end if;
 prizes = (select battle_prizes from geobr_private.natal_config where id = 1);
 for r in select * from geobr_private.natal_arena_players where arena_id = a.id and active and not disqualified
   order by finished desc, case when a.mode = 'platformer' then best_ms end nulls last, percent desc, updated_at loop
  v_place = v_place + 1;
  v_prize = case when not r.bot and (r.finished or (a.mode = 'classic' and r.percent > 0)) then coalesce((prizes ->> (v_place - 1))::int, 0) else 0 end;
  update geobr_private.natal_arena_players set place = v_place, prize = v_prize where arena_id = a.id and slot = r.slot;
  if v_prize > 0 then perform geobr_private.natal_add_candy(r.user_id, v_prize, 'arena', a.id::text); end if;
 end loop;
 update geobr_private.natal_arena set state = 'finished', ends_at = least(ends_at, now()) where id = a.id;
end $$;

create or replace function geobr_private.natal_arena_json(p_arena bigint) returns jsonb language sql stable security definer set search_path = '' as $$
 select jsonb_build_object('id', a.id, 'state', a.state, 'lobby_until', a.lobby_until, 'starts_at', a.starts_at, 'ends_at', a.ends_at, 'now', now(),
  'mode', a.mode, 'bots', a.bots, 'forced_level', a.forced_level,
  'joined', exists (select 1 from geobr_private.natal_arena_players p where p.arena_id = a.id and p.user_id = auth.uid() and p.active),
  'my_vote', (select p.vote from geobr_private.natal_arena_players p where p.arena_id = a.id and p.user_id = auth.uid() and p.active),
  'level', (select jsonb_build_object('id', l.id, 'name', l.name, 'gmd_path', l.gmd_path, 'file_version', l.file_version, 'platformer', l.platformer, 'kind', l.kind)
    from public.geobr_natal_levels l where l.id = a.level_id),
  'maps', coalesce((select jsonb_agg(jsonb_build_object('id', l.id, 'name', l.name, 'kind', l.kind, 'platformer', l.platformer,
    'votes', (select count(*) from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active and p.vote = l.id)) order by l.kind, l.sort_order, l.id)
    from public.geobr_natal_levels l where l.published), '[]'::jsonb),
  'players', coalesce((select jsonb_agg(jsonb_build_object('name', p.display_name, 'bot', p.bot, 'percent', p.percent, 'best_ms', p.best_ms,
     'finished', p.finished, 'disqualified', p.disqualified, 'place', p.place, 'prize', p.prize, 'me', p.user_id is not distinct from auth.uid() and not p.bot,
     'accessory', p.accessory)
    order by p.place nulls last, p.finished desc, case when a.mode = 'platformer' then p.best_ms end nulls last, p.percent desc, p.joined_at)
    from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active), '[]'::jsonb))
 from geobr_private.natal_arena a where a.id = p_arena $$;

create or replace function geobr_private.natal_arena_state() returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint;
begin
 perform geobr_private.require_access();
 v_id = geobr_private.natal_arena_current();
 if v_id is not null then perform geobr_private.natal_arena_tick(v_id); end if;
 return jsonb_build_object('arena', geobr_private.natal_arena_json(v_id), 'staff', geobr_private.natal_staff());
end $$;

create or replace function geobr_private.natal_arena_blocked(p_mods text[]) returns text[] language sql stable security definer set search_path = '' as $$
 select coalesce(array_agg(m), '{}') from unnest(coalesce(p_mods, '{}')) m
 where lower(m) = any (select lower(g) from unnest((select guard_mods from geobr_private.natal_config where id = 1)) g) $$;

create or replace function geobr_private.natal_arena_join(p_mods text[]) returns jsonb language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players; v_id bigint = geobr_private.natal_arena_current(); blocked text[] = geobr_private.natal_arena_blocked(p_mods);
begin
 p = geobr_private.natal_me();
 if v_id is null or (select state from geobr_private.natal_arena where id = v_id) <> 'lobby' then raise exception 'A entrada da Batalha Natalina esta fechada agora'; end if;
 if cardinality(blocked) > 0 then raise exception 'BattleGuard: desative % para entrar', array_to_string(blocked, ', '); end if;
 if (select count(*) from geobr_private.natal_arena_players where arena_id = v_id and not bot and active) >= 40 then raise exception 'A batalha esta cheia'; end if;
 insert into geobr_private.natal_arena_players(arena_id, slot, user_id, display_name, accessory)
 values (v_id, auth.uid()::text, auth.uid(), geobr_private.collab_display_name(), p.equipped)
 on conflict (arena_id, slot) do update set active = true, display_name = excluded.display_name, accessory = excluded.accessory, updated_at = now();
 return geobr_private.natal_arena_state();
end $$;

create or replace function geobr_private.natal_arena_leave() returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 update geobr_private.natal_arena_players p set active = false, vote = null, updated_at = now() from geobr_private.natal_arena a
  where p.arena_id = a.id and a.state = 'lobby' and p.user_id = auth.uid() and not p.bot;
 return geobr_private.natal_arena_state();
end $$;

create or replace function geobr_private.natal_arena_vote(p_level bigint) returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current();
begin
 perform geobr_private.require_access();
 if not exists (select 1 from public.geobr_natal_levels where id = p_level and published) then raise exception 'Mapa invalido'; end if;
 update geobr_private.natal_arena_players p set vote = p_level, updated_at = now() from geobr_private.natal_arena a
  where a.id = v_id and p.arena_id = a.id and a.state = 'lobby' and p.user_id = auth.uid() and not p.bot and p.active;
 if not found then raise exception 'Entre na batalha para votar (so durante a espera)'; end if;
 return geobr_private.natal_arena_state();
end $$;

-- BattleGuard during the match: a cheat mod from the owner's list disqualifies.
create or replace function geobr_private.natal_arena_guard(p_mods text[]) returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current(); blocked text[] = geobr_private.natal_arena_blocked(p_mods);
begin
 perform geobr_private.require_access();
 update geobr_private.natal_arena_players p set guard_mods = blocked,
  disqualified = disqualified or (cardinality(blocked) > 0 and a.state = 'running'), updated_at = now()
  from geobr_private.natal_arena a where a.id = v_id and p.arena_id = a.id and p.user_id = auth.uid() and not p.bot;
 return jsonb_build_object('ok', cardinality(blocked) = 0, 'blocked', to_jsonb(blocked));
end $$;

create or replace function geobr_private.natal_arena_progress(p_percent int, p_finished boolean, p_ms int) returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current(); a geobr_private.natal_arena;
begin
 perform geobr_private.require_access();
 select * into a from geobr_private.natal_arena where id = v_id;
 if a.state = 'running' and now() >= a.starts_at and now() <= a.ends_at + interval '5 seconds' then
  update geobr_private.natal_arena_players set
   percent = greatest(percent, least(100, greatest(0, coalesce(p_percent, 0)))),
   finished = finished or coalesce(p_finished, false),
   best_ms = case when coalesce(p_finished, false) and p_ms between 1000 and 3600000 then least(coalesce(best_ms, p_ms), p_ms) else best_ms end,
   updated_at = now()
  where arena_id = a.id and user_id = auth.uid() and not bot and active and not disqualified;
 end if;
 return geobr_private.natal_arena_state();
end $$;

-- Event level on a timer (like GD's Event): the published "fase" levels take
-- turns every rotation_hours, counted from the event start; the screen shows
-- a countdown to the next one. Minigames are always open.
create or replace function geobr_private.natal_event_level(p_at timestamptz default now()) returns jsonb language sql stable security definer set search_path = '' as $$
 with c as (select starts_at, greatest(1, rotation_hours) * 3600 as period from geobr_private.natal_config where id = 1),
 f as (select id, row_number() over (order by sort_order, id) - 1 as i, count(*) over () as n
       from public.geobr_natal_levels where published and kind = 'fase'),
 k as (select floor(extract(epoch from p_at - c.starts_at) / c.period)::bigint as slot, c.* from c)
 select jsonb_build_object('id', f.id, 'index', f.i + 1, 'count', f.n, 'period_hours', k.period / 3600,
   'next_at', k.starts_at + make_interval(secs => (k.slot + 1) * k.period))
 from f, k where f.i = ((k.slot % f.n) + f.n) % f.n
$$;

-- Candy for the event level only while it is the current one (30 min grace
-- for a run that started before the change); minigames any time.
create or replace function geobr_private.natal_level_complete(p_level bigint, p_elapsed real) returns jsonb language plpgsql security definer set search_path = '' as $$
declare l public.geobr_natal_levels; given boolean;
begin
 perform geobr_private.natal_me();
 select * into l from public.geobr_natal_levels where id = p_level and published;
 if l.id is null then raise exception 'Fase do evento nao encontrada'; end if;
 if l.kind = 'fase' and p_level is distinct from (geobr_private.natal_event_level() ->> 'id')::bigint
    and p_level is distinct from (geobr_private.natal_event_level(now() - interval '30 minutes') ->> 'id')::bigint then
  return jsonb_build_object('ok', false, 'reason', 'fora', 'state', geobr_private.natal_state());
 end if;
 if coalesce(p_elapsed, 0) < l.min_seconds then return jsonb_build_object('ok', false, 'reason', 'tempo'); end if;
 given = l.candy_reward > 0 and geobr_private.natal_add_candy(auth.uid(), l.candy_reward, 'fase', l.id::text);
 return jsonb_build_object('ok', true, 'candy', case when given then l.candy_reward else 0 end, 'state', geobr_private.natal_state());
end $$;

-- Everything the Natal screen shows (adds staff test mode and the battle).
create or replace function geobr_private.natal_state() returns jsonb language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; p geobr_private.natal_players; today date = geobr_private.natal_today(); v_arena bigint = geobr_private.natal_arena_current();
begin
 perform geobr_private.require_access();
 select * into c from geobr_private.natal_config where id = 1;
 insert into geobr_private.natal_players(user_id) values (auth.uid()) on conflict do nothing;
 select * into p from geobr_private.natal_players where user_id = auth.uid();
 return jsonb_build_object(
  'event', jsonb_build_object('enabled', c.enabled, 'active', geobr_private.natal_active(), 'starts_at', c.starts_at, 'ends_at', c.ends_at,
    'public', c.enabled and now() >= c.starts_at and now() < c.ends_at, 'testing', c.test_mode and geobr_private.natal_staff(), 'test_day', c.test_day,
    'today', today, 'calendar_day', case when extract(month from today) = 12 then least(25, extract(day from today))::int else 0 end,
    'calendar_catchup', c.calendar_catchup,
    'arena', (select a.state from geobr_private.natal_arena a where a.id = v_arena),
    'level', geobr_private.natal_event_level(), 'now', now()),
  'me', jsonb_build_object('candy', p.candy, 'lifetime', p.lifetime, 'equipped', p.equipped, 'banned', p.banned, 'ban_reason', p.ban_reason),
  'is_admin', geobr_private.natal_staff(),
  'inventory', coalesce((select jsonb_agg(accessory_id order by acquired_at) from geobr_private.natal_inventory where user_id = auth.uid()), '[]'::jsonb),
  'spin_today', (select reward from geobr_private.natal_spins where user_id = auth.uid() and day = today),
  'calendar_claimed', coalesce((select jsonb_agg(day order by day) from geobr_private.natal_calendar_claims where user_id = auth.uid()), '[]'::jsonb),
  'calendar', c.calendar, 'roulette', c.roulette, 'box_prices', c.box_prices, 'pass', c.pass,
  'pass_claimed', coalesce((select jsonb_agg(tier order by tier) from geobr_private.natal_pass_claims where user_id = auth.uid()), '[]'::jsonb),
  'levels_done', coalesce((select jsonb_agg(ref::bigint) from geobr_private.natal_ledger where user_id = auth.uid() and reason = 'fase'), '[]'::jsonb));
end $$;

-- Owner tools (staff only, server-checked), every action logged.
create or replace function geobr_private.natal_admin(p_action text, p_data jsonb) returns jsonb language plpgsql security definer set search_path = '' as $$
declare target uuid; result jsonb = '{}'::jsonb; d jsonb = coalesce(p_data, '{}'::jsonb); v_id bigint; c geobr_private.natal_config;
begin
 if not geobr_private.natal_staff() then raise exception 'So o Dono e a staff de eventos usam esse painel'; end if;
 if d ? 'player' then
  select supabase_user into target from geobr_gdps.accounts where lower(username) = lower(trim(d ->> 'player')) limit 1;
  if target is null then raise exception 'Jogador nao encontrado (ou sem conta GeoBR vinculada)'; end if;
 end if;
 if p_action = 'config' then
  if (d ->> 'test_day')::int not between 0 and 25 then raise exception 'Dia de teste vai de 0 a 25'; end if;
  if (d ->> 'lobby_seconds')::int not between 30 and 900 then raise exception 'Espera vai de 30 a 900 segundos'; end if;
  if (d ->> 'rotation_hours')::int not between 1 and 168 then raise exception 'Troca da fase vai de 1 a 168 horas'; end if;
  update geobr_private.natal_config set
   enabled = coalesce((d ->> 'enabled')::boolean, enabled),
   test_mode = coalesce((d ->> 'test_mode')::boolean, test_mode),
   test_day = coalesce((d ->> 'test_day')::int, test_day),
   lobby_seconds = coalesce((d ->> 'lobby_seconds')::int, lobby_seconds),
   rotation_hours = coalesce((d ->> 'rotation_hours')::int, rotation_hours),
   starts_at = coalesce((d ->> 'starts_at')::timestamptz, starts_at),
   ends_at = coalesce((d ->> 'ends_at')::timestamptz, ends_at),
   calendar_catchup = coalesce((d ->> 'calendar_catchup')::boolean, calendar_catchup),
   box_prices = coalesce(d -> 'box_prices', box_prices), duplicate_refund = coalesce(d -> 'duplicate_refund', duplicate_refund),
   roulette = coalesce(d -> 'roulette', roulette), calendar = coalesce(d -> 'calendar', calendar), pass = coalesce(d -> 'pass', pass),
   battle_prizes = coalesce(d -> 'battle_prizes', battle_prizes), battle_seconds = coalesce(d -> 'battle_seconds', battle_seconds),
   guard_mods = coalesce((select array_agg(x) from jsonb_array_elements_text(d -> 'guard_mods') x), guard_mods),
   updated_at = now(), updated_by = auth.uid() where id = 1;
  p_action = 'overview';
 end if;
 if p_action in ('overview', 'get_config') then
  select * into c from geobr_private.natal_config where id = 1;
  result = jsonb_build_object('config', to_jsonb(c) - 'updated_by',
   'players', (select count(*) from geobr_private.natal_players),
   'candy_total', (select coalesce(sum(candy), 0) from geobr_private.natal_players),
   'accessories_owned', (select count(*) from geobr_private.natal_inventory),
   'levels_published', (select count(*) from public.geobr_natal_levels where published),
   'levels_total', (select count(*) from public.geobr_natal_levels),
   'banned', (select count(*) from geobr_private.natal_players where banned),
   'event_level', geobr_private.natal_event_level(),
   'arena', geobr_private.natal_arena_json(geobr_private.natal_arena_current()));
 elsif p_action = 'grant' then
  if target is null then raise exception 'Informe o jogador'; end if;
  if coalesce((d ->> 'amount')::bigint, 0) = 0 then raise exception 'Informe a quantidade de Candy'; end if;
  perform geobr_private.natal_add_candy(target, (d ->> 'amount')::bigint, 'dono', gen_random_uuid()::text);
  select jsonb_build_object('candy', candy) into result from geobr_private.natal_players where user_id = target;
 elsif p_action = 'give_accessory' then
  if target is null then raise exception 'Informe o jogador'; end if;
  result = geobr_private.natal_give(target, jsonb_build_object('kind', 'accessory', 'id', d ->> 'accessory'), 'dono', gen_random_uuid()::text);
 elsif p_action = 'ban' then
  if target is null then raise exception 'Informe o jogador'; end if;
  insert into geobr_private.natal_players(user_id) values (target) on conflict do nothing;
  update geobr_private.natal_players set banned = coalesce((d ->> 'banned')::boolean, true), ban_reason = left(d ->> 'reason', 120), updated_at = now() where user_id = target;
  if coalesce((d ->> 'banned')::boolean, true) then
   update geobr_private.natal_battle_players p set disqualified = true from geobr_private.natal_battles b
    where p.battle_id = b.id and b.state <> 'finished' and p.user_id = target;
   update geobr_private.natal_arena_players p set disqualified = true from geobr_private.natal_arena a
    where p.arena_id = a.id and a.state in ('lobby','running') and p.user_id = target;
  end if;
  result = jsonb_build_object('banned', coalesce((d ->> 'banned')::boolean, true));
 elsif p_action = 'player' then
  if target is null then raise exception 'Informe o jogador'; end if;
  insert into geobr_private.natal_players(user_id) values (target) on conflict do nothing;
  select jsonb_build_object('candy', p.candy, 'lifetime', p.lifetime, 'banned', p.banned, 'reason', p.ban_reason, 'equipped', p.equipped,
   'accessories', (select count(*) from geobr_private.natal_inventory i where i.user_id = target),
   'recent', (select jsonb_agg(jsonb_build_object('delta', l.delta, 'reason', l.reason, 'at', l.created_at) order by l.id desc)
     from (select * from geobr_private.natal_ledger where user_id = target order by id desc limit 8) l))
  into result from geobr_private.natal_players p where p.user_id = target;
 elsif p_action = 'arena_open' then
  if exists (select 1 from geobr_private.natal_arena where state in ('lobby','running')) then raise exception 'Ja tem uma Batalha Natalina aberta'; end if;
  if not exists (select 1 from public.geobr_natal_levels where published) then raise exception 'Publique pelo menos uma fase ou minigame do Natal primeiro'; end if;
  insert into geobr_private.natal_arena(opened_by, lobby_until, forced_level)
  values (auth.uid(), now() + make_interval(secs => coalesce((d ->> 'lobby_seconds')::int, (select lobby_seconds from geobr_private.natal_config where id = 1))),
   nullif((d ->> 'level')::bigint, 0))
  returning id into v_id;
  perform geobr_private.natal_arena_add_bots(v_id, coalesce((d ->> 'bots')::int, 0));
  result = geobr_private.natal_arena_json(v_id);
 elsif p_action in ('arena_bots', 'arena_start', 'arena_close') then
  select id into v_id from geobr_private.natal_arena where state in ('lobby','running') order by id desc limit 1;
  if v_id is null then raise exception 'Nenhuma Batalha Natalina aberta'; end if;
  if p_action = 'arena_bots' then
   if (select state from geobr_private.natal_arena where id = v_id) <> 'lobby' then raise exception 'Bots so mudam durante a espera'; end if;
   perform geobr_private.natal_arena_add_bots(v_id, coalesce((d ->> 'bots')::int, 0));
  elsif p_action = 'arena_start' then
   update geobr_private.natal_arena set lobby_until = now() where id = v_id and state = 'lobby';
  else
   update geobr_private.natal_arena set state = 'cancelled', ends_at = now() where id = v_id and state = 'lobby';
   update geobr_private.natal_arena set ends_at = now() where id = v_id and state = 'running';
  end if;
  perform geobr_private.natal_arena_tick(v_id);
  result = geobr_private.natal_arena_json(v_id);
 elsif p_action = 'close_battle' then
  update geobr_private.natal_battles set state = 'finished', finalized = true where code = upper(d ->> 'code');
 else
  raise exception 'Acao desconhecida';
 end if;
 insert into geobr_private.natal_admin_log(admin, action, data) values (auth.uid(), p_action, d);
 return coalesce(result, '{}'::jsonb);
end $$;

-- Public RPC wrappers (authenticated players only) ----------------------------------
do $$
declare f record;
begin
 for f in select * from (values
  ('natal_arena_state', ''), ('natal_arena_join', 'p_mods text[]'), ('natal_arena_leave', ''), ('natal_arena_vote', 'p_level bigint'),
  ('natal_arena_guard', 'p_mods text[]'), ('natal_arena_progress', 'p_percent int, p_finished boolean, p_ms int')) as t(name, sig)
 loop
  execute format('revoke all on function geobr_private.%I(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function geobr_private.%I(%s) to authenticated', f.name, f.sig);
  execute format('create or replace function public.geobr_%s(%s) returns jsonb language sql set search_path = '''' as $f$ select geobr_private.%I(%s) $f$',
   f.name, f.sig, f.name, coalesce((select string_agg(split_part(trim(x), ' ', 1), ', ') from unnest(string_to_array(nullif(f.sig, ''), ',')) x), ''));
  execute format('revoke all on function public.geobr_%s(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function public.geobr_%s(%s) to authenticated', f.name, f.sig);
 end loop;
end $$;
do $$ declare f text; begin
 foreach f in array array['natal_staff()','natal_arena_current()','natal_arena_add_bots(bigint,int)','natal_arena_tick(bigint)','natal_arena_json(bigint)',
  'natal_arena_blocked(text[])','natal_today()','natal_active()','natal_event_level(timestamptz)'] loop
  execute format('revoke all on function geobr_private.%s from public, anon, authenticated', f);
 end loop;
end $$;

insert into geobr_private.releases(version, enabled, message)
values ('1.16.1', true, 'GeoBR 1.16.1: GeoBR Natal 2026 (teste)'), ('1.16.2', true, 'GeoBR 1.16.2: GeoBR Natal 2026 (teste)')
on conflict (version) do nothing;
