-- GeoBR Natal 2026, part 4: Christmas profile themes and profile frames
-- (antlers, wreath, lights, candy cane, snow). They are the event's own
-- cosmetics, kept after the event: from the pass, the Loja de Doces or the
-- owner. Profiles show the equipped ones to everyone. Non-destructive.

create table if not exists public.geobr_natal_cosmetics (
 id text primary key check (id ~ '^[a-z0-9_]{2,40}$'),
 kind text not null check (kind in ('theme','frame')),
 name text not null check (char_length(name) between 1 and 40),
 rarity text not null default 'raro' check (rarity in ('comum','incomum','raro','epico','lendario','extremamente_raro')),
 price int check (price is null or price between 1 and 100000),
 source text not null default 'loja' check (source in ('loja','passe','calendario','dono')),
 sort_order int not null default 0,
 published boolean not null default true
);
alter table public.geobr_natal_cosmetics enable row level security;
grant select on public.geobr_natal_cosmetics to anon, authenticated;
do $$ begin
 if not exists (select 1 from pg_policies where tablename = 'geobr_natal_cosmetics' and policyname = 'natal_cosmetics_read') then
  create policy natal_cosmetics_read on public.geobr_natal_cosmetics for select to anon, authenticated using (published);
 end if;
end $$;
insert into public.geobr_natal_cosmetics(id, kind, name, rarity, price, source, sort_order) values
 ('natal_noite', 'theme', 'Noite Feliz', 'epico', null, 'passe', 1),
 ('natal_floresta', 'theme', 'Floresta Nevada', 'epico', null, 'passe', 2),
 ('natal_bengala', 'theme', 'Bengala Doce', 'raro', 900, 'loja', 3),
 ('natal_lareira', 'theme', 'Lareira Quentinha', 'lendario', 1500, 'loja', 4),
 ('chifres', 'frame', 'Chifres de Rena', 'lendario', null, 'passe', 10),
 ('neve', 'frame', 'Moldura de Neve', 'raro', null, 'passe', 11),
 ('pisca', 'frame', 'Pisca-Pisca', 'epico', null, 'passe', 12),
 ('bengala', 'frame', 'Moldura de Bengala', 'raro', 600, 'loja', 13),
 ('guirlanda', 'frame', 'Guirlanda', 'epico', 900, 'loja', 14)
on conflict (id) do nothing;

create table if not exists geobr_private.natal_cosmetic_inventory (
 user_id uuid not null references auth.users(id) on delete cascade,
 cosmetic_id text not null references public.geobr_natal_cosmetics(id) on delete cascade,
 source text not null,
 acquired_at timestamptz not null default now(),
 primary key (user_id, cosmetic_id)
);
alter table geobr_private.natal_cosmetic_inventory enable row level security;
revoke all on geobr_private.natal_cosmetic_inventory from public, anon, authenticated;
alter table geobr_private.natal_players add column if not exists natal_theme text;
alter table geobr_private.natal_players add column if not exists natal_frame text;

-- Pass: some tiers now give themes and frames (the antlers are the big one).
update geobr_private.natal_config set pass =
 jsonb_set(jsonb_set(jsonb_set(jsonb_set(jsonb_set(pass,
  '{2,reward}', '{"kind":"frame","id":"neve"}'),
  '{5,reward}', '{"kind":"theme","id":"natal_noite"}'),
  '{8,reward}', '{"kind":"frame","id":"pisca"}'),
  '{11,reward}', '{"kind":"theme","id":"natal_floresta"}'),
  '{16,reward}', '{"kind":"frame","id":"chifres"}')
where id = 1 and pass -> 2 -> 'reward' ->> 'kind' = 'candy';

-- Rewards: candy, box, accessory, theme or frame.
create or replace function geobr_private.natal_give(p_user uuid, p_reward jsonb, p_reason text, p_ref text) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare kind text = p_reward ->> 'kind'; a public.geobr_natal_accessories; cz public.geobr_natal_cosmetics;
begin
 if kind = 'candy' then
  perform geobr_private.natal_add_candy(p_user, greatest(0, (p_reward ->> 'amount')::int), p_reason, p_ref);
  return p_reward;
 elsif kind = 'box' then
  return geobr_private.natal_open(p_user, p_reward ->> 'rarity', p_reason || ':' || p_ref) || jsonb_build_object('box', p_reward ->> 'rarity');
 elsif kind = 'accessory' then
  select * into a from public.geobr_natal_accessories where id = p_reward ->> 'id';
  if a.id is null then raise exception 'Acessorio % nao existe', p_reward ->> 'id'; end if;
  insert into geobr_private.natal_inventory(user_id, accessory_id, source) values (p_user, a.id, p_reason) on conflict do nothing;
  return jsonb_build_object('kind', 'accessory', 'id', a.id, 'name', a.name, 'rarity', a.rarity, 'duplicate', false);
 elsif kind in ('theme', 'frame') then
  select * into cz from public.geobr_natal_cosmetics where id = p_reward ->> 'id';
  if cz.id is null then raise exception 'Item % nao existe', p_reward ->> 'id'; end if;
  insert into geobr_private.natal_cosmetic_inventory(user_id, cosmetic_id, source) values (p_user, cz.id, p_reason) on conflict do nothing;
  return jsonb_build_object('kind', cz.kind, 'id', cz.id, 'name', cz.name, 'rarity', cz.rarity, 'duplicate', false);
 end if;
 raise exception 'Premio invalido';
end $$;

-- Everything the Natal screen shows (adds the owned/equipped themes and frames).
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
  'me', jsonb_build_object('candy', p.candy, 'lifetime', p.lifetime, 'equipped', p.equipped, 'banned', p.banned, 'ban_reason', p.ban_reason,
    'theme', p.natal_theme, 'frame', p.natal_frame),
  'is_admin', geobr_private.natal_staff(),
  'inventory', coalesce((select jsonb_agg(accessory_id order by acquired_at) from geobr_private.natal_inventory where user_id = auth.uid()), '[]'::jsonb),
  'cosmetics', coalesce((select jsonb_agg(cosmetic_id order by acquired_at) from geobr_private.natal_cosmetic_inventory where user_id = auth.uid()), '[]'::jsonb),
  'spin_today', (select reward from geobr_private.natal_spins where user_id = auth.uid() and day = today),
  'calendar_claimed', coalesce((select jsonb_agg(day order by day) from geobr_private.natal_calendar_claims where user_id = auth.uid()), '[]'::jsonb),
  'calendar', c.calendar, 'roulette', c.roulette, 'box_prices', c.box_prices, 'pass', c.pass,
  'pass_claimed', coalesce((select jsonb_agg(tier order by tier) from geobr_private.natal_pass_claims where user_id = auth.uid()), '[]'::jsonb),
  'levels_done', coalesce((select jsonb_agg(ref::bigint) from geobr_private.natal_ledger where user_id = auth.uid() and reason = 'fase'), '[]'::jsonb));
end $$;

-- Equip a theme or a frame you own (null removes it). Works after the event too.
create or replace function geobr_private.natal_equip_cosmetic(p_kind text, p_id text) returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.natal_me(false);
 if p_kind not in ('theme', 'frame') then raise exception 'Tipo invalido'; end if;
 if p_id is not null and not exists (select 1 from geobr_private.natal_cosmetic_inventory i join public.geobr_natal_cosmetics c on c.id = i.cosmetic_id
   where i.user_id = auth.uid() and i.cosmetic_id = p_id and c.kind = p_kind) then
  raise exception 'Voce nao tem esse item';
 end if;
 update geobr_private.natal_players set
  natal_theme = case when p_kind = 'theme' then p_id else natal_theme end,
  natal_frame = case when p_kind = 'frame' then p_id else natal_frame end,
  updated_at = now()
 where user_id = auth.uid();
 return geobr_private.natal_state();
end $$;

-- Buy a theme/frame that has a price (Loja de Doces).
create or replace function geobr_private.natal_buy_cosmetic(p_id text) returns jsonb language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players; cz public.geobr_natal_cosmetics; ref text = gen_random_uuid()::text;
begin
 p = geobr_private.natal_me();
 select * into cz from public.geobr_natal_cosmetics where id = p_id and published;
 if cz.id is null or cz.price is null then raise exception 'Esse item nao esta a venda'; end if;
 if exists (select 1 from geobr_private.natal_cosmetic_inventory where user_id = auth.uid() and cosmetic_id = cz.id) then raise exception 'Voce ja tem esse item'; end if;
 if p.candy < cz.price then raise exception 'Candy insuficiente (precisa de %)', cz.price; end if;
 perform geobr_private.natal_add_candy(auth.uid(), -cz.price, 'loja', ref);
 insert into geobr_private.natal_cosmetic_inventory(user_id, cosmetic_id, source) values (auth.uid(), cz.id, 'loja') on conflict do nothing;
 return jsonb_build_object('reward', jsonb_build_object('kind', cz.kind, 'id', cz.id, 'name', cz.name, 'rarity', cz.rarity, 'duplicate', false),
  'state', geobr_private.natal_state());
end $$;

-- What other players wear (profiles, battle list): accessory, theme and frame.
create or replace function geobr_private.natal_equipped(p_accounts bigint[]) returns jsonb language sql stable security definer set search_path = '' as $$
 select coalesce(jsonb_agg(jsonb_build_object('account_id', a.account_id,
   'accessory', (select x.id from public.geobr_natal_accessories x where x.id = p.equipped and x.published),
   'theme', (select c.id from public.geobr_natal_cosmetics c where c.id = p.natal_theme and c.published),
   'frame', (select c.id from public.geobr_natal_cosmetics c where c.id = p.natal_frame and c.published))), '[]'::jsonb)
 from geobr_gdps.accounts a join geobr_private.natal_players p on p.user_id = a.supabase_user
 where a.account_id = any(coalesce(p_accounts, '{}')) and not p.banned
$$;

-- Staff only: gives every published accessory, theme and frame to a player (empty name = me). Logged.
create or replace function geobr_private.natal_admin_unlock(p_player text) returns jsonb language plpgsql security definer set search_path = '' as $$
declare target uuid; n int; m int;
begin
 if not geobr_private.natal_staff() then raise exception 'So o Dono e a staff de eventos usam esse painel'; end if;
 if nullif(trim(coalesce(p_player, '')), '') is null then target = auth.uid();
 else select supabase_user into target from geobr_gdps.accounts where lower(username) = lower(trim(p_player)) limit 1;
 end if;
 if target is null then raise exception 'Jogador nao encontrado (ou sem conta GeoBR vinculada)'; end if;
 insert into geobr_private.natal_players(user_id) values (target) on conflict do nothing;
 insert into geobr_private.natal_inventory(user_id, accessory_id, source)
  select target, id, 'dono' from public.geobr_natal_accessories where published on conflict do nothing;
 get diagnostics n = row_count;
 insert into geobr_private.natal_cosmetic_inventory(user_id, cosmetic_id, source)
  select target, id, 'dono' from public.geobr_natal_cosmetics where published on conflict do nothing;
 get diagnostics m = row_count;
 insert into geobr_private.natal_admin_log(admin, action, data) values (auth.uid(), 'unlock_all', jsonb_build_object('player', coalesce(p_player, ''), 'added', n + m));
 return jsonb_build_object('added', n + m, 'state', case when target = auth.uid() then geobr_private.natal_state() end);
end $$;

do $$
declare f record;
begin
 for f in select * from (values ('natal_equip_cosmetic', 'p_kind text, p_id text'), ('natal_buy_cosmetic', 'p_id text')) as t(name, sig)
 loop
  execute format('revoke all on function geobr_private.%I(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function geobr_private.%I(%s) to authenticated', f.name, f.sig);
  execute format('create or replace function public.geobr_%s(%s) returns jsonb language sql set search_path = '''' as $f$ select geobr_private.%I(%s) $f$',
   f.name, f.sig, f.name, coalesce((select string_agg(split_part(trim(x), ' ', 1), ', ') from unnest(string_to_array(nullif(f.sig, ''), ',')) x), ''));
  execute format('revoke all on function public.geobr_%s(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function public.geobr_%s(%s) to authenticated', f.name, f.sig);
 end loop;
end $$;

insert into geobr_private.releases(version, enabled, message)
values ('1.16.4', true, 'GeoBR 1.16.4: GeoBR Natal 2026 (teste)')
on conflict (version) do nothing;
