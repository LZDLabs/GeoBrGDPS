-- GeoBR Natal 2026, part 8 (final adjustments).
-- 1) The pass follows the Candy you have now (spending lowers it too).
-- 2) Less Candy: top 3 60/35/20, +15 for beating the level, +5 at 50%, smaller
--    roulette amounts, pass up to 4500.
-- 3) Bots: nobody adds them any more; a player alone in a battle gets one dumb bot
--    that plays attempts (dies at different points, restarts, rarely finishes).
-- 4) Live positions for spectating: each player sends its last second of
--    positions; natal_arena_watch returns them (and the bots' current %).

update geobr_private.natal_config set
 battle_prizes = '[60, 35, 20]'::jsonb,
 battle_bonus = '{"finish": 15, "half": 5}'::jsonb,
 roulette = '[{"kind":"candy","amount":5,"weight":30},{"kind":"candy","amount":10,"weight":25},{"kind":"candy","amount":20,"weight":15},{"kind":"candy","amount":40,"weight":6},{"kind":"box","rarity":"comum","weight":12},{"kind":"box","rarity":"incomum","weight":7},{"kind":"box","rarity":"raro","weight":4},{"kind":"box","rarity":"epico","weight":1}]'::jsonb,
 pass = '[{"need":30,"reward":{"kind":"candy","amount":25}},{"need":80,"reward":{"kind":"candy","amount":20}},{"need":140,"reward":{"kind":"box","rarity":"comum"}},{"need":200,"reward":{"kind":"box","rarity":"comum"}},{"need":270,"reward":{"kind":"candy","amount":35}},{"need":350,"reward":{"id":"neve","kind":"frame"}},{"need":430,"reward":{"kind":"box","rarity":"incomum"}},{"need":510,"reward":{"kind":"box","rarity":"incomum"}},{"need":600,"reward":{"kind":"candy","amount":45}},{"need":690,"reward":{"id":"aureola","kind":"accessory"}},{"need":790,"reward":{"kind":"box","rarity":"comum"}},{"need":890,"reward":{"id":"natal_noite","kind":"theme"}},{"need":990,"reward":{"kind":"candy","amount":55}},{"need":1090,"reward":{"kind":"box","rarity":"raro"}},{"need":1200,"reward":{"kind":"box","rarity":"raro"}},{"need":1310,"reward":{"kind":"candy","amount":80}},{"need":1420,"reward":{"kind":"candy","amount":65}},{"need":1530,"reward":{"id":"pisca","kind":"frame"}},{"need":1650,"reward":{"kind":"box","rarity":"incomum"}},{"need":1760,"reward":{"id":"asas_anjo","kind":"accessory"}},{"need":1880,"reward":{"kind":"candy","amount":25}},{"need":2010,"reward":{"kind":"candy","amount":100}},{"need":2130,"reward":{"kind":"box","rarity":"comum"}},{"need":2260,"reward":{"id":"natal_floresta","kind":"theme"}},{"need":2390,"reward":{"kind":"candy","amount":35}},{"need":2510,"reward":{"kind":"candy","amount":120}},{"need":2650,"reward":{"kind":"box","rarity":"incomum"}},{"need":2780,"reward":{"kind":"box","rarity":"epico"}},{"need":2920,"reward":{"kind":"candy","amount":45}},{"need":3050,"reward":{"id":"louros_dourados","kind":"accessory"}},{"need":3190,"reward":{"kind":"box","rarity":"comum"}},{"need":3330,"reward":{"kind":"candy","amount":150}},{"need":3470,"reward":{"kind":"candy","amount":55}},{"need":3620,"reward":{"id":"chifres","kind":"frame"}},{"need":3760,"reward":{"kind":"box","rarity":"raro"}},{"need":3900,"reward":{"id":"coroa_gelo_real","kind":"accessory"}},{"need":4050,"reward":{"kind":"candy","amount":65}},{"need":4200,"reward":{"kind":"box","rarity":"lendario"}},{"need":4350,"reward":{"kind":"box","rarity":"incomum"}},{"need":4500,"reward":{"id":"anel_saturno","kind":"accessory"}}]'::jsonb
where id = 1;

alter table geobr_private.natal_arena_players add column if not exists bot_plan jsonb;
alter table geobr_private.natal_arena_players add column if not exists bot_len real;
alter table geobr_private.natal_arena_players add column if not exists cur_pct int not null default 0;
alter table geobr_private.natal_arena_players add column if not exists trail jsonb;
alter table geobr_private.natal_arena_players add column if not exists trail_at timestamptz;

-- 1) Pass by the Candy you have now.
create or replace function geobr_private.natal_pass_claim(p_tier int) returns jsonb language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players; t jsonb; result jsonb;
begin
 p = geobr_private.natal_me();
 t = (select pass from geobr_private.natal_config where id = 1) -> (p_tier - 1);
 if t is null then raise exception 'Nivel do passe invalido'; end if;
 if p.candy < (t ->> 'need')::int then raise exception 'Voce precisa ter % Candy para pegar esse nivel', (t ->> 'need')::int; end if;
 insert into geobr_private.natal_pass_claims(user_id, tier, at) values (auth.uid(), p_tier, now())
 on conflict (user_id, tier) do update set at = excluded.at where not geobr_private.natal_counts(geobr_private.natal_pass_claims.at);
 if not found then raise exception 'Premio ja resgatado'; end if;
 result = geobr_private.natal_give(auth.uid(), t -> 'reward', 'passe', p_tier::text);
 return jsonb_build_object('reward', result, 'state', geobr_private.natal_state());
end $$;

-- 3) A dumb bot: a list of attempts (where it dies; 100 = beats the level).
create or replace function geobr_private.natal_bot_plan() returns jsonb language plpgsql volatile set search_path = '' as $$
declare n int = 7 + floor(random() * 8)::int; i int; plan jsonb = '[]'::jsonb; pct int;
begin
 for i in 1 .. n loop
  pct = least(96, 3 + floor(power(random(), 1.7) * (40 + i * 6))::int);
  plan = plan || to_jsonb(pct);
 end loop;
 if random() < 0.3 then plan = plan || to_jsonb(100); end if;
 return plan;
end $$;

-- Where a bot is after `elapsed` seconds: current attempt %, best %, finished (and when).
create or replace function geobr_private.natal_bot_step(p_plan jsonb, p_len real, p_elapsed real,
 out cur int, out best int, out finished boolean, out finished_after real) language plpgsql immutable set search_path = '' as $$
declare t real = 0; pct int; dur real; n int = jsonb_array_length(coalesce(p_plan, '[]'::jsonb)); i int = 0; loops int = 0;
begin
 cur = 0; best = 0; finished = false; finished_after = null;
 if n = 0 then return; end if;
 loop
  pct = (p_plan ->> (i % n))::int;
  dur = pct / 100.0 * p_len + 1.5;
  if p_elapsed < t + dur - 1.5 then
   cur = least(pct, floor((p_elapsed - t) / p_len * 100)::int); best = greatest(best, cur); return;
  end if;
  best = greatest(best, pct);
  if pct >= 100 then finished = true; finished_after = t + p_len; cur = 100; return; end if;
  if p_elapsed < t + dur then cur = 0; return; end if;   -- respawning
  t = t + dur; i = i + 1;
  if i % n = 0 then loops = loops + 1; exit when loops > 20; end if;
 end loop;
end $$;

create or replace function geobr_private.natal_arena_tick(p_arena bigint) returns void language plpgsql security definer set search_path = '' as $$
declare a geobr_private.natal_arena; v_level bigint; v_secs int; v_len real; prizes jsonb; bonus jsonb; r record; v_place int = 0; v_prize int; s record; elapsed real;
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
  -- Nobody adds bots: only a player alone gets one (and no extra ones from before).
  update geobr_private.natal_arena_players set active = false where arena_id = a.id and bot;
  if (select count(*) from geobr_private.natal_arena_players where arena_id = a.id and not bot and active) = 1 then
   insert into geobr_private.natal_arena_players(arena_id, slot, bot, display_name, accessory)
   values (a.id, 'bot:1', true, (array['Bot Rena','Bot Elfo','Bot Noel','Bot Floco','Bot Biscoito'])[1 + floor(random() * 5)::int],
    (select id from public.geobr_natal_accessories where published order by random() limit 1))
   on conflict (arena_id, slot) do update set active = true;
   update geobr_private.natal_arena set bots = 1 where id = a.id;
  else
   update geobr_private.natal_arena set bots = 0 where id = a.id;
  end if;
  -- Everyone opens the level now; it starts when all have loaded it (or after 40 s).
  update geobr_private.natal_arena_players set ready = bot where arena_id = a.id;
  update geobr_private.natal_arena set state = 'running', level_id = v_level, mode = 'classic',
   starts_at = null, ends_at = null, load_until = now() + interval '40 seconds' where id = a.id;
  return;
 end if;
 if a.state <> 'running' then return; end if;
 if a.starts_at is null then
  if now() >= a.load_until or not exists (select 1 from geobr_private.natal_arena_players
     where arena_id = a.id and not bot and active and not disqualified and not ready) then
   v_secs = coalesce(((select battle_seconds from geobr_private.natal_config where id = 1) ->> 'classic')::int, 300);
   v_len = greatest(10, coalesce((select min_seconds from public.geobr_natal_levels where id = a.level_id), 20) / 0.9);
   update geobr_private.natal_arena_players set bot_plan = geobr_private.natal_bot_plan(), bot_len = v_len * (1.05 + random() * 0.25)
    where arena_id = a.id and bot and active;
   update geobr_private.natal_arena set starts_at = now() + interval '5 seconds', ends_at = now() + interval '5 seconds' + make_interval(secs => v_secs) where id = a.id;
  end if;
  return;
 end if;
 elapsed = greatest(0, extract(epoch from least(now(), a.ends_at) - a.starts_at));
 for r in select slot, bot_plan, bot_len from geobr_private.natal_arena_players where arena_id = a.id and bot and active and not finished loop
  select * into s from geobr_private.natal_bot_step(r.bot_plan, coalesce(r.bot_len, 30), elapsed);
  update geobr_private.natal_arena_players set cur_pct = s.cur, percent = greatest(percent, s.best), finished = s.finished,
   updated_at = case when s.finished then a.starts_at + make_interval(secs => s.finished_after)
                     when s.best > percent then now() else updated_at end
  where arena_id = a.id and slot = r.slot;
 end loop;
 -- The first to beat the level wins at once; the others keep playing for 2nd/3rd
 -- (and whoever finished can watch) until everyone finished or the time is up.
 if now() < a.ends_at and exists (select 1 from geobr_private.natal_arena_players
    where arena_id = a.id and not bot and active and not disqualified and not finished) then
  return;
 end if;
 prizes = (select battle_prizes from geobr_private.natal_config where id = 1);
 bonus = (select battle_bonus from geobr_private.natal_config where id = 1);
 for r in select * from geobr_private.natal_arena_players where arena_id = a.id and active and not disqualified
   order by finished desc, percent desc, updated_at, joined_at loop
  v_place = v_place + 1;
  v_prize = case when not r.bot and (r.finished or r.percent > 0) then coalesce((prizes ->> (v_place - 1))::int, 0) else 0 end
   + case when r.bot then 0 when r.finished then coalesce((bonus ->> 'finish')::int, 0) when r.percent >= 50 then coalesce((bonus ->> 'half')::int, 0) else 0 end;
  update geobr_private.natal_arena_players set place = v_place, prize = v_prize where arena_id = a.id and slot = r.slot;
  if v_prize > 0 then perform geobr_private.natal_add_candy(r.user_id, v_prize, 'arena', a.id::text); end if;
 end loop;
 update geobr_private.natal_arena set state = 'finished', ends_at = least(ends_at, now()) where id = a.id;
end $$;

-- 4) Live: best %, the current attempt % and the last second of positions
-- (at most 30 samples: [t ms, x, y, rotation, mode, dead]).
create or replace function geobr_private.natal_arena_live(p_percent int, p_cur int, p_trail jsonb) returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current(); a geobr_private.natal_arena; v_pct int = least(99, greatest(0, coalesce(p_percent, 0)));
begin
 perform geobr_private.require_access();
 select * into a from geobr_private.natal_arena where id = v_id;
 if a.state = 'running' and a.starts_at is not null and now() >= a.starts_at and now() <= a.ends_at + interval '5 seconds' then
  update geobr_private.natal_arena_players set
   updated_at = case when not finished and v_pct > percent then now() else updated_at end,
   percent = case when finished then percent else greatest(percent, v_pct) end,
   cur_pct = least(100, greatest(0, coalesce(p_cur, 0))),
   trail = case when jsonb_typeof(p_trail) = 'array' and jsonb_array_length(p_trail) <= 30 then p_trail else trail end,
   trail_at = now()
  where arena_id = a.id and user_id = auth.uid() and not bot and active and ready;
 end if;
 return geobr_private.natal_arena_state();
end $$;

-- For whoever is watching: every player's live data (bots have no positions,
-- only their current attempt %), the age of each trail in ms.
create or replace function geobr_private.natal_arena_watch() returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current();
begin
 perform geobr_private.require_access();
 return jsonb_build_object('now', now(), 'players', coalesce((select jsonb_agg(jsonb_build_object(
   'key', left(md5(p.slot), 10), 'name', p.display_name, 'bot', p.bot, 'cur', p.cur_pct, 'percent', p.percent, 'finished', p.finished,
   'me', p.user_id is not distinct from auth.uid() and not p.bot, 'trail', p.trail,
   'age', case when p.trail_at is null then null else (extract(epoch from now() - p.trail_at) * 1000)::int end)
   order by p.finished desc, p.percent desc, p.joined_at)
  from geobr_private.natal_arena_players p where p.arena_id = v_id and p.active and not p.disqualified), '[]'::jsonb));
end $$;

do $$
declare f record;
begin
 for f in select * from (values ('natal_arena_live', 'p_percent int, p_cur int, p_trail jsonb'), ('natal_arena_watch', '')) as t(name, sig) loop
  execute format('revoke all on function geobr_private.%I(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function geobr_private.%I(%s) to authenticated', f.name, f.sig);
  execute format('create or replace function public.geobr_%s(%s) returns jsonb language sql set search_path = '''' as $f$ select geobr_private.%I(%s) $f$',
   f.name, f.sig, f.name, coalesce((select string_agg(split_part(trim(x), ' ', 1), ', ') from unnest(string_to_array(nullif(f.sig, ''), ',')) x), ''));
  execute format('revoke all on function public.geobr_%s(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function public.geobr_%s(%s) to authenticated', f.name, f.sig);
 end loop;
 revoke all on function geobr_private.natal_bot_plan() from public, anon, authenticated;
 revoke all on function geobr_private.natal_bot_step(jsonb, real, real) from public, anon, authenticated;
end $$;

-- Owner panel: battles open without bots.
create or replace function geobr_private.natal_admin(p_action text, p_data jsonb) returns jsonb language plpgsql security definer set search_path = '' as $$
declare target uuid; result jsonb = '{}'::jsonb; d jsonb = coalesce(p_data, '{}'::jsonb); v_id bigint; c geobr_private.natal_config; v_acc bigint; v_min int; v_lobby int;
begin
 if not geobr_private.natal_staff() then raise exception 'So o Dono e quem ele escolheu usam esse painel'; end if;
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
  -- Minimum time to beat it (anti-cheat): 90% of the shortest run of its GD length (Tiny .. XL).
  insert into public.geobr_natal_levels(kind, sort_order, name, creator, difficulty, platformer, gmd_path, published, min_seconds)
  values ('fase', coalesce((select max(sort_order) + 1 from public.geobr_natal_levels), 1), trim(d ->> 'name'),
   left(coalesce(nullif(trim(d ->> 'creator'), ''), 'Equipe GeoBR'), 40), '', coalesce((d ->> 'platformer')::boolean, false), d ->> 'gmd_path', true,
   case coalesce((d ->> 'length')::int, 1) when 0 then 5 when 1 then 9 when 2 then 27 when 3 then 54 else 108 end);
  p_action = 'overview';
 elsif p_action = 'level_publish' then
  update public.geobr_natal_levels set published = coalesce((d ->> 'published')::boolean, not published), updated_at = now()
   where id = (d ->> 'id')::bigint and kind = 'fase';
  if not found then raise exception 'Fase nao encontrada'; end if;
  p_action = 'overview';
 elsif p_action in ('staff_add', 'staff_remove') then
  if not geobr_private.natal_owner() then raise exception 'So o Dono escolhe quem usa o painel'; end if;
  select account_id into v_acc from geobr_gdps.accounts where lower(username) = lower(trim(d ->> 'player')) order by account_id limit 1;
  if v_acc is null then raise exception 'Jogador nao encontrado'; end if;
  update geobr_private.natal_config set staff_accounts = case when p_action = 'staff_add'
    then (select array_agg(distinct x) from unnest(staff_accounts || v_acc) x) else array_remove(staff_accounts, v_acc) end,
   updated_at = now(), updated_by = auth.uid() where id = 1;
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
   'is_owner', geobr_private.natal_owner(),
   'staff', coalesce((select jsonb_agg(a.username order by a.username) from geobr_gdps.accounts a where a.account_id = any(c.staff_accounts)), '[]'::jsonb),
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
  -- Now (entry open for lobby_seconds) or scheduled: the entry opens lobby_seconds before the start.
  v_min = least(1440, greatest(0, coalesce((d ->> 'minutes')::int, 0)));
  v_lobby = coalesce((d ->> 'lobby_seconds')::int, (select lobby_seconds from geobr_private.natal_config where id = 1), 60);
  insert into geobr_private.natal_arena(opened_by, lobby_until, opens_at, forced_level)
  values (auth.uid(), now() + make_interval(secs => greatest(v_lobby, v_min * 60)),
   now() + make_interval(secs => greatest(0, v_min * 60 - v_lobby)), nullif((d ->> 'level')::bigint, 0))
  returning id into v_id;
  -- No bots from the panel: a player alone gets one when the battle starts.
  if v_min = 0 then perform geobr_private.natal_arena_enter(v_id); end if;
  result = geobr_private.natal_arena_json(v_id);
 elsif p_action in ('arena_bots', 'arena_start', 'arena_close') then
  select id into v_id from geobr_private.natal_arena where state in ('lobby','running') order by id desc limit 1;
  if v_id is null then raise exception 'Nenhuma Batalha Natalina aberta'; end if;
  if p_action = 'arena_bots' then
   raise exception 'Bots sao automaticos: so quem ficar sozinho ganha um';
  elsif p_action = 'arena_start' then
   -- Start now; whoever starts it plays if no one entered yet (so a test with bots runs).
   if not exists (select 1 from geobr_private.natal_arena_players where arena_id = v_id and not bot and active) then
    perform geobr_private.natal_arena_enter(v_id);
   end if;
   update geobr_private.natal_arena set lobby_until = now(), opens_at = least(coalesce(opens_at, now()), now()) where id = v_id and state = 'lobby';
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

insert into geobr_private.releases(version, enabled, message)
values ('1.16.8', true, 'GeoBR 1.16.8: GeoBR Natal 2026 (teste)')
on conflict (version) do nothing;
