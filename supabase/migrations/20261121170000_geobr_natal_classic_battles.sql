-- GeoBR Natal 2026, part 5.
-- 1) No minigames: the three generated maps are gone (their GDPS copies are
--    marked deleted, minigame rows unpublished). The Batalha Natalina is classic
--    only: the maps are the owner's published classic levels, and whoever beats
--    the level first wins (no one in time: furthest % wins).
-- 2) The owner publishes event levels from the Natal hub (one of his levels):
--    the file goes to natal-levels/ in the levels bucket, the row is created here.
-- 3) Everything counts from the event start (Dec 1, 00:00 Brasilia): the staff
--    test day only works before it, and what the staff did while testing
--    (roulette, calendar, pass, Candy) no longer counts once the event opens.
-- Non-destructive: nothing is deleted, old rows are only marked.

-- 1) Minigames out ----------------------------------------------------------------
update geobr_gdps.levels set deleted = true, updated_at = now()
 where account_id = 1 and level_id in (44, 45, 46) and name in ('Torre de Presentes', 'Corrida do Treno', 'Fabrica do Noel');
update public.geobr_natal_levels set published = false, updated_at = now() where kind = 'minigame';

-- Classic maps of the battle (published event levels that are not platformer).
create or replace function geobr_private.natal_battle_maps() returns setof public.geobr_natal_levels language sql stable security definer set search_path = '' as $$
 select * from public.geobr_natal_levels where published and kind = 'fase' and not platformer order by sort_order, id $$;

create or replace function geobr_private.natal_arena_tick(p_arena bigint) returns void language plpgsql security definer set search_path = '' as $$
declare a geobr_private.natal_arena; v_level bigint; v_secs int; total real; elapsed real; prizes jsonb; r record; v_place int = 0; v_prize int;
begin
 select * into a from geobr_private.natal_arena where id = p_arena for update;
 if a.id is null then return; end if;
 if a.state = 'lobby' and now() >= a.lobby_until then
  if not exists (select 1 from geobr_private.natal_arena_players where arena_id = a.id and not bot and active) then
   update geobr_private.natal_arena set state = 'cancelled', ends_at = now() where id = a.id; return;
  end if;
  select m.id into v_level from geobr_private.natal_battle_maps() m where m.id = a.forced_level;
  if v_level is null then
   select p.vote into v_level from geobr_private.natal_arena_players p join geobr_private.natal_battle_maps() m on m.id = p.vote
   where p.arena_id = a.id and p.active group by p.vote order by count(*) desc, random() limit 1;
  end if;
  if v_level is null then select m.id into v_level from geobr_private.natal_battle_maps() m order by random() limit 1; end if;
  if v_level is null then update geobr_private.natal_arena set state = 'cancelled', ends_at = now() where id = a.id; return; end if;
  v_secs = coalesce(((select battle_seconds from geobr_private.natal_config where id = 1) ->> 'classic')::int, 300);
  update geobr_private.natal_arena_players set bot_target = case when random() < 0.5 then 100 else 35 + floor(random() * 60)::int end,
   bot_pace = 0.3 + random() * 0.6 where arena_id = a.id and bot and active;
  update geobr_private.natal_arena set state = 'running', level_id = v_level, mode = 'classic',
   starts_at = now() + interval '10 seconds', ends_at = now() + interval '10 seconds' + make_interval(secs => v_secs) where id = a.id;
  return;
 end if;
 if a.state <> 'running' then return; end if;
 total = greatest(1, extract(epoch from a.ends_at - a.starts_at));
 elapsed = greatest(0, extract(epoch from least(now(), a.ends_at) - a.starts_at));
 update geobr_private.natal_arena_players set
  percent = least(bot_target, floor(bot_target * least(1, elapsed / (bot_pace * total))))::int,
  finished = bot_target = 100 and elapsed >= bot_pace * total,
  updated_at = case when bot_target = 100 and elapsed >= bot_pace * total then a.starts_at + make_interval(secs => bot_pace * total) else now() end
 where arena_id = a.id and bot and active and not finished;
 if now() < a.ends_at
    and not exists (select 1 from geobr_private.natal_arena_players where arena_id = a.id and active and finished and not disqualified)
    and exists (select 1 from geobr_private.natal_arena_players where arena_id = a.id and not bot and active and not disqualified) then
  return;
 end if;
 prizes = (select battle_prizes from geobr_private.natal_config where id = 1);
 for r in select * from geobr_private.natal_arena_players where arena_id = a.id and active and not disqualified
   order by finished desc, percent desc, updated_at, joined_at loop
  v_place = v_place + 1;
  v_prize = case when not r.bot and (r.finished or r.percent > 0) then coalesce((prizes ->> (v_place - 1))::int, 0) else 0 end;
  update geobr_private.natal_arena_players set place = v_place, prize = v_prize where arena_id = a.id and slot = r.slot;
  if v_prize > 0 then perform geobr_private.natal_add_candy(r.user_id, v_prize, 'arena', a.id::text); end if;
 end loop;
 update geobr_private.natal_arena set state = 'finished', ends_at = least(ends_at, now()) where id = a.id;
end $$;

create or replace function geobr_private.natal_arena_json(p_arena bigint) returns jsonb language sql stable security definer set search_path = '' as $$
 select jsonb_build_object('id', a.id, 'state', a.state, 'lobby_until', a.lobby_until, 'starts_at', a.starts_at, 'ends_at', a.ends_at, 'now', now(),
  'mode', 'classic', 'bots', a.bots, 'forced_level', a.forced_level,
  'joined', exists (select 1 from geobr_private.natal_arena_players p where p.arena_id = a.id and p.user_id = auth.uid() and p.active),
  'my_vote', (select p.vote from geobr_private.natal_arena_players p where p.arena_id = a.id and p.user_id = auth.uid() and p.active),
  'level', (select jsonb_build_object('id', l.id, 'name', l.name, 'gmd_path', l.gmd_path, 'file_version', l.file_version, 'platformer', l.platformer, 'kind', l.kind)
    from public.geobr_natal_levels l where l.id = a.level_id),
  'winner', (select p.display_name from geobr_private.natal_arena_players p where p.arena_id = a.id and p.place = 1 and p.finished),
  'maps', coalesce((select jsonb_agg(jsonb_build_object('id', m.id, 'name', m.name, 'kind', m.kind, 'platformer', m.platformer,
    'votes', (select count(*) from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active and p.vote = m.id)))
    from geobr_private.natal_battle_maps() m), '[]'::jsonb),
  'players', coalesce((select jsonb_agg(jsonb_build_object('name', p.display_name, 'bot', p.bot, 'percent', p.percent, 'best_ms', p.best_ms,
     'finished', p.finished, 'disqualified', p.disqualified, 'place', p.place, 'prize', p.prize, 'me', p.user_id is not distinct from auth.uid() and not p.bot,
     'accessory', p.accessory)
    order by p.place nulls last, p.finished desc, p.percent desc, p.updated_at, p.joined_at)
    from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active), '[]'::jsonb))
 from geobr_private.natal_arena a where a.id = p_arena $$;

create or replace function geobr_private.natal_arena_vote(p_level bigint) returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current();
begin
 perform geobr_private.require_access();
 if not exists (select 1 from geobr_private.natal_battle_maps() m where m.id = p_level) then raise exception 'Mapa invalido'; end if;
 update geobr_private.natal_arena_players p set vote = p_level, updated_at = now() from geobr_private.natal_arena a
  where a.id = v_id and p.arena_id = a.id and a.state = 'lobby' and p.user_id = auth.uid() and not p.bot and p.active;
 if not found then raise exception 'Entre na batalha para votar (so durante a espera)'; end if;
 return geobr_private.natal_arena_state();
end $$;

-- updated_at = when the player reached their best % (or beat the level), the tie-break.
create or replace function geobr_private.natal_arena_progress(p_percent int, p_finished boolean, p_ms int) returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current(); a geobr_private.natal_arena; v_pct int = least(100, greatest(0, coalesce(p_percent, 0)));
begin
 perform geobr_private.require_access();
 select * into a from geobr_private.natal_arena where id = v_id;
 if a.state = 'running' and now() >= a.starts_at and now() <= a.ends_at + interval '5 seconds' then
  update geobr_private.natal_arena_players set
   updated_at = case when (coalesce(p_finished, false) and not finished) or (not finished and v_pct > percent) then now() else updated_at end,
   percent = case when coalesce(p_finished, false) then 100 else greatest(percent, v_pct) end,
   finished = finished or coalesce(p_finished, false),
   best_ms = case when coalesce(p_finished, false) and p_ms between 1000 and 3600000 then least(coalesce(best_ms, p_ms), p_ms) else best_ms end
  where arena_id = a.id and user_id = auth.uid() and not bot and active and not disqualified;
 end if;
 return geobr_private.natal_arena_state();
end $$;

-- 3) Everything counts from the event start ---------------------------------------
alter table geobr_private.natal_players add column if not exists season_start timestamptz;
alter table geobr_private.natal_spins add column if not exists at timestamptz not null default now();

-- A claim/spin made before the event opened (staff tests) stops counting when it opens.
create or replace function geobr_private.natal_counts(p_at timestamptz) returns boolean language sql stable security definer set search_path = '' as $$
 select coalesce((select now() < c.starts_at or p_at >= c.starts_at from geobr_private.natal_config c where c.id = 1), true) $$;

-- The simulated December day of the staff test mode only exists before the start.
create or replace function geobr_private.natal_today() returns date language sql stable security definer set search_path = '' as $$
 select case when c.test_mode and c.test_day between 1 and 25 and now() < c.starts_at and geobr_private.natal_staff()
  then make_date(extract(year from c.starts_at at time zone 'America/Sao_Paulo')::int, 12, c.test_day)
  else (now() at time zone 'America/Sao_Paulo')::date end
 from geobr_private.natal_config c where c.id = 1 $$;

-- Once per player, the first time after the start: what was earned while testing
-- is renamed in the ledger (frees "once" rewards) and Candy is recounted from the
-- start. Accessories, themes and frames stay.
create or replace function geobr_private.natal_season(p_user uuid) returns void language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; tag text;
begin
 select * into c from geobr_private.natal_config where id = 1;
 if c.id is null or now() < c.starts_at or p_user is null then return; end if;
 if exists (select 1 from geobr_private.natal_players where user_id = p_user and season_start is not distinct from c.starts_at) then return; end if;
 tag = 'teste' || to_char(c.starts_at, 'YYYYMMDD') || '_';
 if exists (select 1 from geobr_private.natal_ledger where user_id = p_user and created_at < c.starts_at and reason !~ '^teste') then
  update geobr_private.natal_ledger set reason = tag || reason where user_id = p_user and created_at < c.starts_at and reason !~ '^teste';
  update geobr_private.natal_players set
   candy = greatest(0, coalesce((select sum(delta) from geobr_private.natal_ledger where user_id = p_user and created_at >= c.starts_at), 0)),
   lifetime = coalesce((select sum(greatest(0, delta)) from geobr_private.natal_ledger where user_id = p_user and created_at >= c.starts_at), 0),
   updated_at = now()
  where user_id = p_user;
 end if;
 update geobr_private.natal_players set season_start = c.starts_at where user_id = p_user;
end $$;

create or replace function geobr_private.natal_me(p_need_active boolean default true) returns geobr_private.natal_players
language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players;
begin
 perform geobr_private.require_access();
 insert into geobr_private.natal_players(user_id) values (auth.uid()) on conflict do nothing;
 perform geobr_private.natal_season(auth.uid());
 select * into p from geobr_private.natal_players where user_id = auth.uid() for update;
 if p.banned then raise exception 'Voce esta suspenso do evento de Natal: %', coalesce(p.ban_reason, 'fale com a staff'); end if;
 if p_need_active and not geobr_private.natal_active() then raise exception 'O evento de Natal nao esta aberto agora'; end if;
 return p;
end $$;

create or replace function geobr_private.natal_spin() returns jsonb language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; total int; pick int; seg jsonb; acc int = 0; result jsonb; today date = geobr_private.natal_today(); idx int = 0;
begin
 perform geobr_private.natal_me();
 if exists (select 1 from geobr_private.natal_spins where user_id = auth.uid() and day = today and geobr_private.natal_counts(at)) then
  raise exception 'Voce ja girou a roleta hoje';
 end if;
 select * into c from geobr_private.natal_config where id = 1;
 select sum((s ->> 'weight')::int) into total from jsonb_array_elements(c.roulette) s;
 pick = floor(random() * total)::int;
 for seg in select * from jsonb_array_elements(c.roulette) loop
  acc = acc + (seg ->> 'weight')::int;
  exit when pick < acc;
  idx = idx + 1;
 end loop;
 result = geobr_private.natal_give(auth.uid(), seg - 'weight', 'roleta', today::text) || jsonb_build_object('segment', idx);
 insert into geobr_private.natal_spins(user_id, day, reward, at) values (auth.uid(), today, result, now())
 on conflict (user_id, day) do update set reward = excluded.reward, at = excluded.at;
 return jsonb_build_object('reward', result, 'state', geobr_private.natal_state());
end $$;

-- Calendar: day N opens on December N (Brasilia time), from Dec 1 to Dec 25.
create or replace function geobr_private.natal_calendar_claim(p_day int) returns jsonb language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; today date = geobr_private.natal_today(); open_day int; result jsonb;
begin
 perform geobr_private.natal_me();
 select * into c from geobr_private.natal_config where id = 1;
 open_day = case when extract(month from today) = 12 then least(25, extract(day from today))::int else 0 end;
 if p_day is null or p_day < 1 or p_day > 25 or p_day > open_day then raise exception 'Esse dia ainda nao abriu'; end if;
 if not c.calendar_catchup and p_day <> open_day then raise exception 'So da para abrir o dia de hoje'; end if;
 if exists (select 1 from geobr_private.natal_calendar_claims where user_id = auth.uid() and day = p_day and geobr_private.natal_counts(at)) then
  raise exception 'Dia ja aberto';
 end if;
 result = geobr_private.natal_give(auth.uid(), c.calendar -> (p_day - 1), 'calendario', p_day::text);
 insert into geobr_private.natal_calendar_claims(user_id, day, reward, at) values (auth.uid(), p_day, result, now())
 on conflict (user_id, day) do update set reward = excluded.reward, at = excluded.at;
 return jsonb_build_object('reward', result, 'state', geobr_private.natal_state());
end $$;

create or replace function geobr_private.natal_pass_claim(p_tier int) returns jsonb language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players; t jsonb; result jsonb;
begin
 p = geobr_private.natal_me();
 t = (select pass from geobr_private.natal_config where id = 1) -> (p_tier - 1);
 if t is null then raise exception 'Nivel do passe invalido'; end if;
 if p.lifetime < (t ->> 'need')::int then raise exception 'Junte % Candy no evento para liberar', (t ->> 'need')::int; end if;
 insert into geobr_private.natal_pass_claims(user_id, tier, at) values (auth.uid(), p_tier, now())
 on conflict (user_id, tier) do update set at = excluded.at where not geobr_private.natal_counts(geobr_private.natal_pass_claims.at);
 if not found then raise exception 'Premio ja resgatado'; end if;
 result = geobr_private.natal_give(auth.uid(), t -> 'reward', 'passe', p_tier::text);
 return jsonb_build_object('reward', result, 'state', geobr_private.natal_state());
end $$;

create or replace function geobr_private.natal_state() returns jsonb language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; p geobr_private.natal_players; today date = geobr_private.natal_today(); v_arena bigint = geobr_private.natal_arena_current();
begin
 perform geobr_private.require_access();
 select * into c from geobr_private.natal_config where id = 1;
 insert into geobr_private.natal_players(user_id) values (auth.uid()) on conflict do nothing;
 perform geobr_private.natal_season(auth.uid());
 select * into p from geobr_private.natal_players where user_id = auth.uid();
 return jsonb_build_object(
  'event', jsonb_build_object('enabled', c.enabled, 'active', geobr_private.natal_active(), 'starts_at', c.starts_at, 'ends_at', c.ends_at,
    'public', c.enabled and now() >= c.starts_at and now() < c.ends_at, 'testing', c.test_mode and now() < c.starts_at and geobr_private.natal_staff(), 'test_day', c.test_day,
    'today', today, 'calendar_day', case when extract(month from today) = 12 then least(25, extract(day from today))::int else 0 end,
    'calendar_catchup', c.calendar_catchup,
    'arena', (select a.state from geobr_private.natal_arena a where a.id = v_arena),
    'level', geobr_private.natal_event_level(), 'now', now()),
  'me', jsonb_build_object('candy', p.candy, 'lifetime', p.lifetime, 'equipped', p.equipped, 'banned', p.banned, 'ban_reason', p.ban_reason,
    'theme', p.natal_theme, 'frame', p.natal_frame),
  'is_admin', geobr_private.natal_staff(),
  'inventory', coalesce((select jsonb_agg(accessory_id order by acquired_at) from geobr_private.natal_inventory where user_id = auth.uid()), '[]'::jsonb),
  'cosmetics', coalesce((select jsonb_agg(cosmetic_id order by acquired_at) from geobr_private.natal_cosmetic_inventory where user_id = auth.uid()), '[]'::jsonb),
  'spin_today', (select reward from geobr_private.natal_spins where user_id = auth.uid() and day = today and geobr_private.natal_counts(at)),
  'calendar_claimed', coalesce((select jsonb_agg(day order by day) from geobr_private.natal_calendar_claims where user_id = auth.uid() and geobr_private.natal_counts(at)), '[]'::jsonb),
  'calendar', c.calendar, 'roulette', c.roulette, 'box_prices', c.box_prices, 'pass', c.pass,
  'pass_claimed', coalesce((select jsonb_agg(tier order by tier) from geobr_private.natal_pass_claims where user_id = auth.uid() and geobr_private.natal_counts(at)), '[]'::jsonb),
  'levels_done', coalesce((select jsonb_agg(ref::bigint) from geobr_private.natal_ledger where user_id = auth.uid() and reason = 'fase'), '[]'::jsonb));
end $$;

-- 2) Owner tools: publish one of his levels, list / show / hide event levels,
-- battle length; the rest is unchanged.
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
  if (d -> 'battle_seconds' ->> 'classic')::int not between 60 and 1800 then raise exception 'A batalha dura de 1 a 30 minutos'; end if;
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
   battle_prizes = coalesce(d -> 'battle_prizes', battle_prizes),
   battle_seconds = case when d -> 'battle_seconds' ? 'classic' then battle_seconds || jsonb_build_object('classic', (d -> 'battle_seconds' ->> 'classic')::int) else battle_seconds end,
   guard_mods = coalesce((select array_agg(x) from jsonb_array_elements_text(d -> 'guard_mods') x), guard_mods),
   updated_at = now(), updated_by = auth.uid() where id = 1;
  p_action = 'overview';
 elsif p_action = 'level_add' then
  if coalesce(d ->> 'gmd_path', '') !~ '^natal-levels/[A-Za-z0-9_-]+\.gmd$' then raise exception 'Arquivo da fase invalido'; end if;
  if char_length(trim(coalesce(d ->> 'name', ''))) not between 1 and 64 then raise exception 'Nome da fase invalido'; end if;
  insert into public.geobr_natal_levels(kind, sort_order, name, creator, difficulty, platformer, gmd_path, published)
  values ('fase', coalesce((select max(sort_order) + 1 from public.geobr_natal_levels), 1), trim(d ->> 'name'),
   left(coalesce(nullif(trim(d ->> 'creator'), ''), 'Equipe GeoBR'), 40), '', coalesce((d ->> 'platformer')::boolean, false), d ->> 'gmd_path', true);
  p_action = 'overview';
 elsif p_action = 'level_publish' then
  update public.geobr_natal_levels set published = coalesce((d ->> 'published')::boolean, not published), updated_at = now()
   where id = (d ->> 'id')::bigint and kind = 'fase';
  if not found then raise exception 'Fase nao encontrada'; end if;
  p_action = 'overview';
 end if;
 if p_action in ('overview', 'get_config') then
  select * into c from geobr_private.natal_config where id = 1;
  result = jsonb_build_object('config', to_jsonb(c) - 'updated_by',
   'players', (select count(*) from geobr_private.natal_players),
   'candy_total', (select coalesce(sum(candy), 0) from geobr_private.natal_players),
   'accessories_owned', (select count(*) from geobr_private.natal_inventory),
   'levels_published', (select count(*) from public.geobr_natal_levels where published and kind = 'fase'),
   'levels_total', (select count(*) from public.geobr_natal_levels where kind = 'fase'),
   'levels', coalesce((select jsonb_agg(jsonb_build_object('id', l.id, 'name', l.name, 'creator', l.creator, 'published', l.published, 'platformer', l.platformer)
     order by l.published desc, l.sort_order, l.id) from public.geobr_natal_levels l where l.kind = 'fase'), '[]'::jsonb),
   'banned', (select count(*) from geobr_private.natal_players where banned),
   'event_level', geobr_private.natal_event_level(),
   'arena', geobr_private.natal_arena_json(geobr_private.natal_arena_current()));
 elsif p_action = 'grant' then
  if target is null then raise exception 'Informe o jogador'; end if;
  if coalesce((d ->> 'amount')::bigint, 0) = 0 then raise exception 'Informe a quantidade de Candy'; end if;
  perform geobr_private.natal_season(target);
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
  perform geobr_private.natal_season(target);
  select jsonb_build_object('candy', p.candy, 'lifetime', p.lifetime, 'banned', p.banned, 'reason', p.ban_reason, 'equipped', p.equipped,
   'accessories', (select count(*) from geobr_private.natal_inventory i where i.user_id = target),
   'recent', (select jsonb_agg(jsonb_build_object('delta', l.delta, 'reason', l.reason, 'at', l.created_at) order by l.id desc)
     from (select * from geobr_private.natal_ledger where user_id = target order by id desc limit 8) l))
  into result from geobr_private.natal_players p where p.user_id = target;
 elsif p_action = 'arena_open' then
  if exists (select 1 from geobr_private.natal_arena where state in ('lobby','running')) then raise exception 'Ja tem uma Batalha Natalina aberta'; end if;
  if not exists (select 1 from geobr_private.natal_battle_maps()) then raise exception 'Publique pelo menos uma fase classica do Natal primeiro (Menu do Dono > Fases)'; end if;
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

-- The level file of a published level: the staff uploads it to natal-levels/ (insert only).
grant execute on function geobr_private.natal_staff() to authenticated;
do $$ begin
 if not exists (select 1 from pg_policies where schemaname = 'storage' and tablename = 'objects' and policyname = 'geobr_natal_staff_level_upload') then
  create policy geobr_natal_staff_level_upload on storage.objects for insert to authenticated
  with check (bucket_id = 'geobr-main-levels' and name ~ '^natal-levels/[A-Za-z0-9_-]+\.gmd$' and (select geobr_private.natal_staff()));
 end if;
end $$;

do $$ declare f text; begin
 foreach f in array array['natal_battle_maps()','natal_counts(timestamptz)','natal_season(uuid)'] loop
  execute format('revoke all on function geobr_private.%s from public, anon, authenticated', f);
 end loop;
end $$;

insert into geobr_private.releases(version, enabled, message)
values ('1.16.5', true, 'GeoBR 1.16.5: GeoBR Natal 2026 (teste)')
on conflict (version) do nothing;
