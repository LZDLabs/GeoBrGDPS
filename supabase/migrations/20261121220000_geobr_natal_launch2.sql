-- GeoBR Natal 2026, part 7 (launch prep 2).
-- 1) Accessories placed from their real art (crowns/earmuffs/antlers on the head,
--    scarves and lights wrapping the icon; new scarf and lights art in the mod).
-- 2) Batalha Natalina every hour by itself (auto_battle_minutes, 0 = off), entry
--    1 min before; the first to beat the level wins and the others keep playing
--    for 2nd/3rd (finished players can watch). Candy for everyone who played.
-- 3) Bigger pass: 40 tiers up to 6000 Candy (reachable by Dec 25 playing battles).
-- 4) Images in news and notices (public bucket geobr-news-images, admins upload).

update public.geobr_natal_accessories set scale = 0.537, off_x = 0.66, off_y = 0.66 where id = 'anel_saturno';
update public.geobr_natal_accessories set scale = 2.0, off_x = 0.0, off_y = 0.1 where id = 'asas_anjo';
update public.geobr_natal_accessories set scale = 2.0, off_x = 0.0, off_y = 0.1 where id = 'asas_morcego';
update public.geobr_natal_accessories set scale = 0.914, off_x = 0.0, off_y = 0.861 where id = 'aureola';
update public.geobr_natal_accessories set scale = 0.64, off_x = -0.2, off_y = 0.69 where id = 'azevinho';
update public.geobr_natal_accessories set scale = 1.032, off_x = 0.0, off_y = -0.515 where id = 'barba_noel';
update public.geobr_natal_accessories set scale = 0.792, off_x = -0.82, off_y = -0.12 where id = 'bengala_doce';
update public.geobr_natal_accessories set scale = 0.786, off_x = -0.82, off_y = -0.117 where id = 'boneco_gengibre';
update public.geobr_natal_accessories set scale = 0.78, off_x = -0.82, off_y = -0.12 where id = 'boneco_neve';
update public.geobr_natal_accessories set scale = 1.16, off_x = 0.0, off_y = -0.372 where id = 'cachecol_listrado';
update public.geobr_natal_accessories set scale = 1.16, off_x = 0.0, off_y = -0.372 where id = 'cachecol_xadrez';
update public.geobr_natal_accessories set scale = 1.008, off_x = 0.004, off_y = 0.762 where id = 'cartola_azevinho';
update public.geobr_natal_accessories set scale = 1.016, off_x = 0.0, off_y = 0.753 where id = 'cartola_floco';
update public.geobr_natal_accessories set scale = 1.115, off_x = 0.0, off_y = 0.765 where id = 'chifres_diabo';
update public.geobr_natal_accessories set scale = 1.012, off_x = 0.0, off_y = 0.779 where id = 'coroa_abismo';
update public.geobr_natal_accessories set scale = 0.996, off_x = 0.0, off_y = 0.789 where id = 'coroa_bengala';
update public.geobr_natal_accessories set scale = 0.996, off_x = 0.0, off_y = 0.789 where id = 'coroa_doce';
update public.geobr_natal_accessories set scale = 0.996, off_x = 0.0, off_y = 0.766 where id = 'coroa_dourada';
update public.geobr_natal_accessories set scale = 1.004, off_x = -0.004, off_y = 0.757 where id = 'coroa_espinhos';
update public.geobr_natal_accessories set scale = 0.996, off_x = 0.0, off_y = 0.746 where id = 'coroa_gelo';
update public.geobr_natal_accessories set scale = 0.996, off_x = 0.0, off_y = 0.746 where id = 'coroa_gelo_real';
update public.geobr_natal_accessories set scale = 1.012, off_x = 0.0, off_y = 0.815 where id = 'coroa_infernal';
update public.geobr_natal_accessories set scale = 1.012, off_x = 0.0, off_y = 0.724 where id = 'coroa_rosa';
update public.geobr_natal_accessories set scale = 1.012, off_x = 0.0, off_y = 0.815 where id = 'coroa_roxa';
update public.geobr_natal_accessories set scale = 0.996, off_x = 0.0, off_y = 0.812 where id = 'coroa_sombria';
update public.geobr_natal_accessories set scale = 0.528, off_x = 0.66, off_y = 0.66 where id = 'estrela_cadente';
update public.geobr_natal_accessories set scale = 0.55, off_x = 0.66, off_y = 0.66 where id = 'estrela_dourada';
update public.geobr_natal_accessories set scale = 0.541, off_x = 0.66, off_y = 0.664 where id = 'floco_neve';
update public.geobr_natal_accessories set scale = 1.23, off_x = 0.045, off_y = 0.691 where id = 'gorro_elfo';
update public.geobr_natal_accessories set scale = 1.239, off_x = 0.07, off_y = 0.699 where id = 'gorro_papai_noel';
update public.geobr_natal_accessories set scale = 0.578, off_x = 0.0, off_y = -0.36 where id = 'gravata_borboleta';
update public.geobr_natal_accessories set scale = 0.786, off_x = -0.003, off_y = -0.397 where id = 'guirlanda';
update public.geobr_natal_accessories set scale = 0.737, off_x = 0.197, off_y = 0.722 where id = 'laco_sombrio';
update public.geobr_natal_accessories set scale = 0.726, off_x = 0.197, off_y = 0.712 where id = 'laco_vermelho';
update public.geobr_natal_accessories set scale = 0.737, off_x = 0.203, off_y = 0.714 where id = 'laco_xadrez';
update public.geobr_natal_accessories set scale = 1.342, off_x = 0.0, off_y = 0.647 where id = 'louros_dourados';
update public.geobr_natal_accessories set scale = 0.792, off_x = -0.817, off_y = -0.12 where id = 'meia_natal';
update public.geobr_natal_accessories set scale = 1.321, off_x = 0.0, off_y = -0.074 where id = 'pisca_pisca';
update public.geobr_natal_accessories set scale = 0.537, off_x = 0.66, off_y = 0.66 where id = 'planeta_roxo';
update public.geobr_natal_accessories set scale = 0.799, off_x = -0.82, off_y = -0.117 where id = 'presente';
update public.geobr_natal_accessories set scale = 1.562, off_x = -0.006, off_y = 0.359 where id = 'protetor_orelha';
update public.geobr_natal_accessories set scale = 1.097, off_x = -0.82, off_y = -0.12 where id = 'rena';
update public.geobr_natal_accessories set scale = 0.476, off_x = 0.0, off_y = -0.39 where id = 'sino_dourado';
update public.geobr_natal_accessories set scale = 0.655, off_x = -0.003, off_y = -0.477 where id = 'sinos_natal';
update public.geobr_natal_accessories set scale = 1.371, off_x = 0.0, off_y = 0.669 where id = 'tiara_rena';

alter table geobr_private.natal_config add column if not exists auto_battle_minutes int not null default 60;
alter table geobr_private.natal_config add column if not exists battle_bonus jsonb not null default '{"finish": 40, "half": 15}';
alter table geobr_private.natal_arena add column if not exists auto boolean not null default false;
update geobr_private.natal_config set pass = '[{"need":40,"reward":{"kind":"candy","amount":25}},{"need":110,"reward":{"kind":"candy","amount":20}},{"need":180,"reward":{"kind":"box","rarity":"comum"}},{"need":270,"reward":{"kind":"box","rarity":"comum"}},{"need":360,"reward":{"kind":"candy","amount":35}},{"need":460,"reward":{"id":"neve","kind":"frame"}},{"need":570,"reward":{"kind":"box","rarity":"incomum"}},{"need":680,"reward":{"kind":"box","rarity":"incomum"}},{"need":800,"reward":{"kind":"candy","amount":45}},{"need":920,"reward":{"id":"aureola","kind":"accessory"}},{"need":1050,"reward":{"kind":"box","rarity":"comum"}},{"need":1180,"reward":{"id":"natal_noite","kind":"theme"}},{"need":1320,"reward":{"kind":"candy","amount":55}},{"need":1450,"reward":{"kind":"box","rarity":"raro"}},{"need":1600,"reward":{"kind":"box","rarity":"raro"}},{"need":1740,"reward":{"kind":"candy","amount":80}},{"need":1890,"reward":{"kind":"candy","amount":65}},{"need":2040,"reward":{"id":"pisca","kind":"frame"}},{"need":2200,"reward":{"kind":"box","rarity":"incomum"}},{"need":2350,"reward":{"id":"asas_anjo","kind":"accessory"}},{"need":2510,"reward":{"kind":"candy","amount":25}},{"need":2680,"reward":{"kind":"candy","amount":100}},{"need":2840,"reward":{"kind":"box","rarity":"comum"}},{"need":3010,"reward":{"id":"natal_floresta","kind":"theme"}},{"need":3180,"reward":{"kind":"candy","amount":35}},{"need":3350,"reward":{"kind":"candy","amount":120}},{"need":3530,"reward":{"kind":"box","rarity":"incomum"}},{"need":3710,"reward":{"kind":"box","rarity":"epico"}},{"need":3890,"reward":{"kind":"candy","amount":45}},{"need":4070,"reward":{"id":"louros_dourados","kind":"accessory"}},{"need":4250,"reward":{"kind":"box","rarity":"comum"}},{"need":4440,"reward":{"kind":"candy","amount":150}},{"need":4630,"reward":{"kind":"candy","amount":55}},{"need":4820,"reward":{"id":"chifres","kind":"frame"}},{"need":5010,"reward":{"kind":"box","rarity":"raro"}},{"need":5200,"reward":{"id":"coroa_gelo_real","kind":"accessory"}},{"need":5400,"reward":{"kind":"candy","amount":65}},{"need":5600,"reward":{"kind":"box","rarity":"lendario"}},{"need":5800,"reward":{"kind":"box","rarity":"incomum"}},{"need":6000,"reward":{"id":"anel_saturno","kind":"accessory"}}]'::jsonb where id = 1;

-- Next automatic battle: on the next slot of auto_battle_minutes counted from
-- midnight (Brasilia), while the event is open (or before it, in staff test mode).
create or replace function geobr_private.natal_auto_schedule() returns void language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; v_day timestamptz; v_slot timestamptz; v_last timestamptz;
begin
 select * into c from geobr_private.natal_config where id = 1;
 if c.id is null or c.auto_battle_minutes <= 0 then return; end if;
 if not ((c.enabled and now() >= c.starts_at and now() < c.ends_at) or (c.test_mode and now() < c.starts_at)) then return; end if;
 perform pg_advisory_xact_lock(81226);
 if exists (select 1 from geobr_private.natal_arena where state in ('lobby', 'running')) then return; end if;
 if not exists (select 1 from geobr_private.natal_battle_maps()) then return; end if;
 select max(coalesce(ends_at, lobby_until)) into v_last from geobr_private.natal_arena;
 if v_last is not null and v_last > now() - interval '2 minutes' then return; end if;   -- results stay on screen a bit
 v_day = date_trunc('day', now() at time zone 'America/Sao_Paulo') at time zone 'America/Sao_Paulo';
 v_slot = v_day + make_interval(mins => c.auto_battle_minutes * (floor(extract(epoch from now() - v_day) / 60 / c.auto_battle_minutes)::int + 1));
 if c.enabled and v_slot >= c.ends_at then return; end if;
 insert into geobr_private.natal_arena(opened_by, lobby_until, opens_at, auto)
 values (null, v_slot, v_slot - make_interval(secs => c.lobby_seconds), true);
end $$;

create or replace function geobr_private.natal_arena_state() returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint;
begin
 perform geobr_private.require_access();
 perform geobr_private.natal_auto_schedule();
 v_id = geobr_private.natal_arena_current();
 if v_id is not null then perform geobr_private.natal_arena_tick(v_id); end if;
 return jsonb_build_object('arena', geobr_private.natal_arena_json(v_id), 'staff', geobr_private.natal_staff());
end $$;

create or replace function geobr_private.natal_arena_tick(p_arena bigint) returns void language plpgsql security definer set search_path = '' as $$
declare a geobr_private.natal_arena; v_level bigint; v_secs int; total real; elapsed real; prizes jsonb; bonus jsonb; r record; v_place int = 0; v_prize int;
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
  -- Top 3 prizes, plus Candy for everyone who played: beat the level / reached 50%.
  v_prize = case when not r.bot and (r.finished or r.percent > 0) then coalesce((prizes ->> (v_place - 1))::int, 0) else 0 end
   + case when r.bot then 0 when r.finished then coalesce((bonus ->> 'finish')::int, 0) when r.percent >= 50 then coalesce((bonus ->> 'half')::int, 0) else 0 end;
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
  'winner', (select p.display_name from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active and p.finished and not p.disqualified order by p.updated_at, p.joined_at limit 1),
  'my_finished', coalesce((select p.finished from geobr_private.natal_arena_players p where p.arena_id = a.id and p.user_id = auth.uid() and p.active and not p.bot), false),
  'auto', a.auto, 'guard', to_jsonb((select guard_mods from geobr_private.natal_config where id = 1)),
  'maps', coalesce((select jsonb_agg(jsonb_build_object('id', m.id, 'name', m.name, 'kind', m.kind, 'platformer', m.platformer,
    'votes', (select count(*) from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active and p.vote = m.id)))
    from geobr_private.natal_battle_maps() m), '[]'::jsonb),
  'players', coalesce((select jsonb_agg(jsonb_build_object('name', p.display_name, 'bot', p.bot, 'percent', p.percent, 'best_ms', p.best_ms,
     'finished', p.finished, 'disqualified', p.disqualified, 'place', p.place, 'prize', p.prize, 'me', p.user_id is not distinct from auth.uid() and not p.bot,
     'accessory', p.accessory, 'ready', p.ready)
    order by p.place nulls last, p.finished desc, p.percent desc, p.updated_at, p.joined_at)
    from geobr_private.natal_arena_players p where p.arena_id = a.id and p.active), '[]'::jsonb))
 from geobr_private.natal_arena a where a.id = p_arena $$;

-- "Level loaded": the battle is moved along first (the entry may have just closed).
create or replace function geobr_private.natal_arena_ready() returns jsonb language plpgsql security definer set search_path = '' as $$
declare v_id bigint = geobr_private.natal_arena_current();
begin
 perform geobr_private.require_access();
 if v_id is not null then perform geobr_private.natal_arena_tick(v_id); end if;
 update geobr_private.natal_arena_players p set ready = true from geobr_private.natal_arena a
  where a.id = v_id and p.arena_id = a.id and a.state = 'running' and p.user_id = auth.uid() and not p.bot and p.active;
 return geobr_private.natal_arena_state();
end $$;

-- Owner: automatic battles on/off and interval (config action keeps the rest).
create or replace function geobr_private.natal_set_auto_battle(p_minutes int) returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 if not geobr_private.natal_staff() then raise exception 'So o Dono e quem ele escolheu usam esse painel'; end if;
 if p_minutes not in (0, 30, 60, 120, 180) then raise exception 'Use 0, 30, 60, 120 ou 180 minutos'; end if;
 update geobr_private.natal_config set auto_battle_minutes = p_minutes, updated_at = now(), updated_by = auth.uid() where id = 1;
 insert into geobr_private.natal_admin_log(admin, action, data) values (auth.uid(), 'auto_battle', jsonb_build_object('minutes', p_minutes));
 return geobr_private.natal_admin('overview', '{}'::jsonb);
end $$;

do $$ begin
 revoke all on function geobr_private.natal_set_auto_battle(int) from public, anon;
 grant execute on function geobr_private.natal_set_auto_battle(int) to authenticated;
 create or replace function public.geobr_natal_set_auto_battle(p_minutes int) returns jsonb language sql set search_path = ''
  as $f$ select geobr_private.natal_set_auto_battle(p_minutes) $f$;
 revoke all on function public.geobr_natal_set_auto_battle(int) from public, anon;
 grant execute on function public.geobr_natal_set_auto_battle(int) to authenticated;
 revoke all on function geobr_private.natal_auto_schedule() from public, anon, authenticated;
end $$;

-- Images in news and notices --------------------------------------------------------
alter table public.geobr_news add column if not exists image_path text
 check (image_path is null or image_path ~ '^news/[A-Za-z0-9_-]+[.](png|jpg|jpeg)$');
alter table public.geobr_announcements add column if not exists image_path text
 check (image_path is null or image_path ~ '^news/[A-Za-z0-9_-]+[.](png|jpg|jpeg)$');
insert into storage.buckets (id, name, public, file_size_limit)
values ('geobr-news-images', 'geobr-news-images', true, 3145728)
on conflict (id) do nothing;
do $$ begin
 if not exists (select 1 from pg_policies where schemaname = 'storage' and tablename = 'objects' and policyname = 'geobr_news_images_admin_upload') then
  create policy geobr_news_images_admin_upload on storage.objects for insert to authenticated
  with check (bucket_id = 'geobr-news-images' and name ~ '^news/[A-Za-z0-9_-]+[.](png|jpg|jpeg)$' and (select geobr_private.is_admin()));
 end if;
end $$;

insert into geobr_private.releases(version, enabled, message)
values ('1.16.7', true, 'GeoBR 1.16.7: GeoBR Natal 2026 (teste)')
on conflict (version) do nothing;

-- The news list (view) also carries the picture.
create or replace view public.geobr_bulletins as
 SELECT 'news:'::text || geobr_news.id::text AS id, geobr_news.title, geobr_news.description, geobr_news.author, geobr_news.type,
    geobr_news.pinned, geobr_news.priority, geobr_news.published, geobr_news.created_at, geobr_news.image_path
   FROM geobr_news WHERE geobr_news.published
UNION ALL
 SELECT 'notice:'::text || geobr_announcements.id::text AS id, geobr_announcements.title, geobr_announcements.message AS description,
    'Equipe GeoBR'::text AS author, 'notice'::text AS type, true AS pinned, 100::smallint AS priority, geobr_announcements.published,
    geobr_announcements.created_at, geobr_announcements.image_path
   FROM geobr_announcements WHERE geobr_announcements.published AND geobr_announcements.expires_at > now();
