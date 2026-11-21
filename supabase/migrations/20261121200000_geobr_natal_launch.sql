-- GeoBR Natal 2026, part 6 (launch prep).
-- 1) Menu do Dono only for the owner's account and the accounts he picks (GDPS
--    roles no longer give it). 2) Entry wait 1 min; battles can be scheduled
--    (the entry opens 1 min before the start, players are notified at 1 h, 5 min
--    and 1 min); after the vote everyone opens the level and it only starts when
--    all have loaded it (max 40 s), then a 5 s countdown. Whoever starts the
--    battle plays when no one entered yet (bot tests work). 3) Candy ranking: by
--    Candy collected in the event; the top 1 when it ends is the event winner.

alter table geobr_private.natal_config add column if not exists owner_account bigint not null default 1;
alter table geobr_private.natal_config add column if not exists staff_accounts bigint[] not null default '{}';
alter table geobr_private.natal_config alter column lobby_seconds set default 60;
update geobr_private.natal_config set lobby_seconds = 60 where id = 1;
alter table geobr_private.natal_arena add column if not exists opens_at timestamptz;
alter table geobr_private.natal_arena add column if not exists load_until timestamptz;
alter table geobr_private.natal_arena_players add column if not exists ready boolean not null default false;

create or replace function geobr_private.natal_owner() returns boolean language sql stable security definer set search_path = '' as $$
 select auth.uid() is not null and exists (select 1 from geobr_gdps.accounts a join geobr_private.natal_config c on c.id = 1
  where a.supabase_user = auth.uid() and a.account_id = c.owner_account) $$;

create or replace function geobr_private.natal_staff() returns boolean language sql stable security definer set search_path = '' as $$
 select auth.uid() is not null and exists (select 1 from geobr_gdps.accounts a join geobr_private.natal_config c on c.id = 1
  where a.supabase_user = auth.uid() and (a.account_id = c.owner_account or a.account_id = any(c.staff_accounts))) $$;

-- The caller enters the battle (join, or the owner starting it).
create or replace function geobr_private.natal_arena_enter(p_arena bigint) returns void language plpgsql security definer set search_path = '' as $$
begin
 insert into geobr_private.natal_players(user_id) values (auth.uid()) on conflict do nothing;
 insert into geobr_private.natal_arena_players(arena_id, slot, user_id, display_name, accessory)
 values (p_arena, auth.uid()::text, auth.uid(), geobr_private.collab_display_name(), (select equipped from geobr_private.natal_players where user_id = auth.uid()))
 on conflict (arena_id, slot) do update set active = true, display_name = excluded.display_name, accessory = excluded.accessory, updated_at = now();
end $$;

create or replace function geobr_private.natal_arena_join(p_mods text[]) returns jsonb language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players; v_id bigint = geobr_private.natal_arena_current(); a geobr_private.natal_arena; blocked text[] = geobr_private.natal_arena_blocked(p_mods);
begin
 p = geobr_private.natal_me();
 select * into a from geobr_private.natal_arena where id = v_id;
 if a.id is null or a.state <> 'lobby' then raise exception 'A entrada da Batalha Natalina esta fechada agora'; end if;
 if now() < coalesce(a.opens_at, a.created_at) then
  raise exception 'A entrada abre as % (horario de Brasilia)', to_char(a.opens_at at time zone 'America/Sao_Paulo', 'HH24:MI');
 end if;
 if cardinality(blocked) > 0 then raise exception 'BattleGuard: desative % para entrar', array_to_string(blocked, ', '); end if;
 if (select count(*) from geobr_private.natal_arena_players where arena_id = v_id and not bot and active) >= 40 then raise exception 'A batalha esta cheia'; end if;
 perform geobr_private.natal_arena_enter(v_id);
 return geobr_private.natal_arena_state();
end $$;

-- The level finished loading on this player's game.
create or replace function geobr_private.natal_arena_ready() returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.require_access();
 update geobr_private.natal_arena_players p set ready = true from geobr_private.natal_arena a
  where a.id = geobr_private.natal_arena_current() and p.arena_id = a.id and a.state = 'running' and p.user_id = auth.uid() and not p.bot and p.active;
 return geobr_private.natal_arena_state();
end $$;

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
   update geobr_private.natal_arena_players set bot_target = case when random() < 0.5 then 100 else 35 + floor(random() * 60)::int end,
    bot_pace = 0.3 + random() * 0.6 where arena_id = a.id and bot and active;
   update geobr_private.natal_arena set starts_at = now() + interval '5 seconds', ends_at = now() + interval '5 seconds' + make_interval(secs => v_secs) where id = a.id;
  end if;
  return;
 end if;
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
  'mode', 'classic', 'bots', a.bots, 'forced_level', a.forced_level, 'opens_at', coalesce(a.opens_at, a.created_at), 'load_until', a.load_until,
  'loading', a.state = 'running' and a.starts_at is null,
  'humans', (select count(*) from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active and not p.bot),
  'ready', (select count(*) from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active and not p.bot and p.ready),
  'my_ready', coalesce((select p.ready from geobr_private.natal_arena_players p where p.arena_id = a.id and p.user_id = auth.uid() and p.active and not p.bot), false),
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
     'accessory', p.accessory, 'ready', p.ready)
    order by p.place nulls last, p.finished desc, p.percent desc, p.updated_at, p.joined_at)
    from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active), '[]'::jsonb))
 from geobr_private.natal_arena a where a.id = p_arena $$;

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
  perform geobr_private.natal_arena_add_bots(v_id, coalesce((d ->> 'bots')::int, 0));
  if v_min = 0 then perform geobr_private.natal_arena_enter(v_id); end if;
  result = geobr_private.natal_arena_json(v_id);
 elsif p_action in ('arena_bots', 'arena_start', 'arena_close') then
  select id into v_id from geobr_private.natal_arena where state in ('lobby','running') order by id desc limit 1;
  if v_id is null then raise exception 'Nenhuma Batalha Natalina aberta'; end if;
  if p_action = 'arena_bots' then
   if (select state from geobr_private.natal_arena where id = v_id) <> 'lobby' then raise exception 'Bots so mudam durante a espera'; end if;
   perform geobr_private.natal_arena_add_bots(v_id, coalesce((d ->> 'bots')::int, 0));
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

-- Anti-cheat for "first to beat the level wins" -----------------------------------
-- BattleGuard also blocks a sped-up game (the client reports it as geobr.speedhack).
create or replace function geobr_private.natal_arena_blocked(p_mods text[]) returns text[] language sql stable security definer set search_path = '' as $$
 select coalesce(array_agg(m), '{}') from unnest(coalesce(p_mods, '{}')) m
 where lower(m) = 'geobr.speedhack' or lower(m) = any (select lower(g) from unnest((select guard_mods from geobr_private.natal_config where id = 1)) g) $$;

-- Progress (%) only; beating the level goes through natal_arena_finish.
create or replace function geobr_private.natal_arena_progress(p_percent int, p_finished boolean, p_ms int) returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current(); a geobr_private.natal_arena; v_pct int = least(99, greatest(0, coalesce(p_percent, 0)));
begin
 perform geobr_private.require_access();
 select * into a from geobr_private.natal_arena where id = v_id;
 if a.state = 'running' and a.starts_at is not null and now() >= a.starts_at and now() <= a.ends_at + interval '5 seconds' then
  update geobr_private.natal_arena_players set
   updated_at = case when not finished and v_pct > percent then now() else updated_at end,
   percent = case when finished then percent else greatest(percent, v_pct) end
  where arena_id = a.id and user_id = auth.uid() and not bot and active and not disqualified and ready;
 end if;
 return geobr_private.natal_arena_state();
end $$;

-- Beat the level: mods checked again, the run must be possible (at least the
-- level's minimum time since the start, on the server clock), and the game must
-- not run faster than real time. Anything else disqualifies.
create or replace function geobr_private.natal_arena_finish(p_ms int, p_wall_ms int, p_mods text[]) returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current(); a geobr_private.natal_arena; v_min int; blocked text[] = geobr_private.natal_arena_blocked(p_mods); why text;
begin
 perform geobr_private.require_access();
 select * into a from geobr_private.natal_arena where id = v_id;
 if a.state = 'running' and a.starts_at is not null and now() >= a.starts_at and now() <= a.ends_at + interval '5 seconds' then
  v_min = greatest(5, coalesce((select min_seconds from public.geobr_natal_levels where id = a.level_id), 20));
  why = case
   when cardinality(blocked) > 0 then 'mod: ' || array_to_string(blocked, ', ')
   when now() - a.starts_at < make_interval(secs => v_min) then 'tempo impossivel'
   when coalesce(p_wall_ms, 0) < v_min * 1000 or coalesce(p_ms, 0) < v_min * 1000 then 'tempo impossivel'
   when p_ms > p_wall_ms * 1.15 + 1500 then 'jogo acelerado'
   when p_wall_ms > extract(epoch from now() - a.starts_at) * 1000 + 3000 then 'tempo invalido'
  end;
  if why is not null then
   update geobr_private.natal_arena_players set disqualified = true, guard_mods = array[why], updated_at = now()
   where arena_id = a.id and user_id = auth.uid() and not bot and active;
  else
   update geobr_private.natal_arena_players set updated_at = case when not finished then now() else updated_at end,
    percent = 100, finished = true, best_ms = least(coalesce(best_ms, p_ms), p_ms)
   where arena_id = a.id and user_id = auth.uid() and not bot and active and not disqualified and ready;
  end if;
 end if;
 return geobr_private.natal_arena_state() || jsonb_build_object('rejected', why);
end $$;

do $$ begin
 revoke all on function geobr_private.natal_arena_finish(int, int, text[]) from public, anon;
 grant execute on function geobr_private.natal_arena_finish(int, int, text[]) to authenticated;
 create or replace function public.geobr_natal_arena_finish(p_ms int, p_wall_ms int, p_mods text[]) returns jsonb language sql set search_path = ''
  as $f$ select geobr_private.natal_arena_finish(p_ms, p_wall_ms, p_mods) $f$;
 revoke all on function public.geobr_natal_arena_finish(int, int, text[]) from public, anon;
 grant execute on function public.geobr_natal_arena_finish(int, int, text[]) to authenticated;
end $$;


-- Candy ranking: Candy collected in the event (spending does not lower it); ties
-- go to who got there first. Top 5 are shown apart; the top 1 at the end wins.
create or replace function geobr_private.natal_ranking(p_offset int) returns jsonb language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; v_off int = greatest(0, coalesce(p_offset, 0)); result jsonb;
begin
 perform geobr_private.require_access();
 select * into c from geobr_private.natal_config where id = 1;
 if now() >= c.starts_at then
  perform geobr_private.natal_season(p.user_id) from geobr_private.natal_players p where p.season_start is distinct from c.starts_at;
 end if;
 with r as (
  select p.user_id, p.lifetime,
   coalesce((select a.username from geobr_gdps.accounts a where a.supabase_user = p.user_id order by a.account_id limit 1), 'Jogador') as name,
   row_number() over (order by p.lifetime desc,
    (select max(l.created_at) from geobr_private.natal_ledger l where l.user_id = p.user_id and l.delta > 0 and l.reason !~ '^teste') nulls last, p.user_id) as pos
  from geobr_private.natal_players p where not p.banned and p.lifetime > 0)
 select jsonb_build_object(
  'top', coalesce((select jsonb_agg(jsonb_build_object('pos', pos, 'name', name, 'candy', lifetime, 'me', user_id = auth.uid()) order by pos) from r where pos <= 5), '[]'::jsonb),
  'rows', coalesce((select jsonb_agg(jsonb_build_object('pos', pos, 'name', name, 'candy', lifetime, 'me', user_id = auth.uid()) order by pos)
    from r where pos > 5 + v_off and pos <= 5 + v_off + 8), '[]'::jsonb),
  'me', (select jsonb_build_object('pos', pos, 'candy', lifetime) from r where user_id = auth.uid()),
  'total', (select count(*) from r),
  'ended', now() >= c.ends_at, 'ends_at', c.ends_at, 'starts_at', c.starts_at)
 into result;
 return result;
end $$;

do $$
declare f record;
begin
 for f in select * from (values ('natal_arena_ready', ''), ('natal_ranking', 'p_offset int')) as t(name, sig) loop
  execute format('revoke all on function geobr_private.%I(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function geobr_private.%I(%s) to authenticated', f.name, f.sig);
  execute format('create or replace function public.geobr_%s(%s) returns jsonb language sql set search_path = '''' as $f$ select geobr_private.%I(%s) $f$',
   f.name, f.sig, f.name, coalesce((select string_agg(split_part(trim(x), ' ', 1), ', ') from unnest(string_to_array(nullif(f.sig, ''), ',')) x), ''));
  execute format('revoke all on function public.geobr_%s(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function public.geobr_%s(%s) to authenticated', f.name, f.sig);
 end loop;
end $$;
revoke all on function geobr_private.natal_owner() from public, anon, authenticated;
revoke all on function geobr_private.natal_arena_enter(bigint) from public, anon, authenticated;

insert into geobr_private.releases(version, enabled, message)
values ('1.16.6', true, 'GeoBR 1.16.6: GeoBR Natal 2026 (teste)')
on conflict (version) do nothing;
