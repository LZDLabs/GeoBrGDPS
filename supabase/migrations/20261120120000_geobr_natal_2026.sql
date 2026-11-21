-- GeoBR Natal 2026 (event). Candy currency, icon accessories and accessory
-- boxes, daily roulette, calendar until Dec 25, Natal pass, event levels and
-- minigames (chosen by the owner like the Main Levels), battle tables (the
-- Batalha Natalina itself is in 20261120140000), and owner tools with a log. The event starts DISABLED; the
-- owner turns it on in the GeoBR panel. Non-destructive: new objects only.

-- Configuration (single row) -------------------------------------------------
create table if not exists geobr_private.natal_config (
 id int primary key default 1 check (id = 1),
 enabled boolean not null default false,
 starts_at timestamptz not null default '2026-12-01 03:00+00',
 ends_at timestamptz not null default '2026-12-27 03:00+00',
 calendar_catchup boolean not null default true,
 box_prices jsonb not null default '{"comum":60,"incomum":120,"raro":220,"epico":400,"lendario":700,"extremamente_raro":1200}',
 duplicate_refund jsonb not null default '{"comum":15,"incomum":30,"raro":55,"epico":100,"lendario":175,"extremamente_raro":300}',
 roulette jsonb not null default '[{"kind":"candy","amount":10,"weight":30},{"kind":"candy","amount":25,"weight":25},{"kind":"candy","amount":50,"weight":15},{"kind":"candy","amount":100,"weight":6},{"kind":"box","rarity":"comum","weight":12},{"kind":"box","rarity":"incomum","weight":7},{"kind":"box","rarity":"raro","weight":4},{"kind":"box","rarity":"epico","weight":1}]',
 calendar jsonb not null default '[{"kind":"candy","amount":20},{"kind":"candy","amount":25},{"kind":"box","rarity":"comum"},{"kind":"candy","amount":30},{"kind":"box","rarity":"incomum"},{"kind":"candy","amount":35},{"kind":"candy","amount":40},{"kind":"box","rarity":"comum"},{"kind":"candy","amount":45},{"kind":"box","rarity":"raro"},{"kind":"candy","amount":50},{"kind":"accessory","id":"boneco_neve"},{"kind":"candy","amount":55},{"kind":"box","rarity":"incomum"},{"kind":"box","rarity":"raro"},{"kind":"candy","amount":60},{"kind":"candy","amount":65},{"kind":"box","rarity":"epico"},{"kind":"candy","amount":70},{"kind":"box","rarity":"raro"},{"kind":"candy","amount":80},{"kind":"box","rarity":"epico"},{"kind":"candy","amount":100},{"kind":"box","rarity":"lendario"},{"kind":"accessory","id":"estrela_cadente"}]',
 pass jsonb not null default '[{"need":50,"reward":{"kind":"candy","amount":20}},{"need":120,"reward":{"kind":"box","rarity":"comum"}},{"need":200,"reward":{"kind":"candy","amount":40}},{"need":300,"reward":{"kind":"box","rarity":"incomum"}},{"need":420,"reward":{"kind":"accessory","id":"aureola"}},{"need":560,"reward":{"kind":"candy","amount":60}},{"need":720,"reward":{"kind":"box","rarity":"raro"}},{"need":900,"reward":{"kind":"candy","amount":80}},{"need":1100,"reward":{"kind":"box","rarity":"raro"}},{"need":1320,"reward":{"kind":"accessory","id":"asas_anjo"}},{"need":1560,"reward":{"kind":"candy","amount":100}},{"need":1820,"reward":{"kind":"box","rarity":"epico"}},{"need":2100,"reward":{"kind":"candy","amount":120}},{"need":2400,"reward":{"kind":"box","rarity":"epico"}},{"need":2720,"reward":{"kind":"accessory","id":"louros_dourados"}},{"need":3060,"reward":{"kind":"candy","amount":150}},{"need":3420,"reward":{"kind":"box","rarity":"lendario"}},{"need":3800,"reward":{"kind":"accessory","id":"coroa_gelo_real"}},{"need":4200,"reward":{"kind":"box","rarity":"lendario"}},{"need":4700,"reward":{"kind":"accessory","id":"anel_saturno"}}]',
 battle_prizes jsonb not null default '[150,90,50]',
 battle_seconds jsonb not null default '{"classic":180,"platformer":300}',
 guard_mods text[] not null default '{absolllute.megahack,thesillydoggo.qolmod,tobyadd.gdh,firee.prism,prevter.openhack,maxnut.gdh}',
 updated_at timestamptz not null default now(),
 updated_by uuid
);
insert into geobr_private.natal_config(id) values (1) on conflict do nothing;

-- Accessories -------------------------------------------------------------------
create table if not exists public.geobr_natal_accessories (
 id text primary key check (id ~ '^[a-z0-9_]{2,40}$'),
 name text not null check (char_length(name) between 1 and 40),
 rarity text not null check (rarity in ('comum','incomum','raro','epico','lendario','extremamente_raro')),
 source text not null default 'caixa' check (source in ('caixa','passe','calendario','dono')),
 slot text not null check (slot in ('head','halo','face','neck','back','float','side')),
 sprite text not null check (sprite ~ '^natal_acc_[a-z0-9_]+\.png$'),
 off_x real not null default 0 check (off_x between -3 and 3),
 off_y real not null default 0 check (off_y between -3 and 3),
 scale real not null default 1 check (scale between 0.1 and 4),
 behind boolean not null default false,
 sort_order int not null default 0,
 published boolean not null default true
);
alter table public.geobr_natal_accessories enable row level security;
grant select on public.geobr_natal_accessories to anon, authenticated;
grant insert, update, delete on public.geobr_natal_accessories to authenticated;
do $$ begin
 if not exists (select 1 from pg_policies where tablename = 'geobr_natal_accessories' and policyname = 'natal_accessories_read') then
  create policy natal_accessories_read on public.geobr_natal_accessories for select to anon, authenticated using (published);
  create policy natal_accessories_admin on public.geobr_natal_accessories for all to authenticated using (geobr_private.is_admin()) with check (geobr_private.is_admin());
 end if;
end $$;
insert into public.geobr_natal_accessories(id, name, rarity, source, slot, sprite, off_x, off_y, scale, behind, sort_order) values
 ('gorro_papai_noel', 'Gorro de Papai Noel', 'comum', 'caixa', 'head', 'natal_acc_gorro_papai_noel.png', 0, 0.55, 1.0, false, 1),
 ('tiara_rena', 'Tiara de Rena', 'comum', 'caixa', 'head', 'natal_acc_tiara_rena.png', 0, 0.55, 1.0, false, 2),
 ('gorro_elfo', 'Gorro de Elfo', 'comum', 'caixa', 'head', 'natal_acc_gorro_elfo.png', 0, 0.55, 1.0, false, 3),
 ('bengala_doce', 'Bengala Doce', 'comum', 'caixa', 'side', 'natal_acc_bengala_doce.png', -0.78, -0.2, 0.6, false, 4),
 ('estrela_dourada', 'Estrela Dourada', 'incomum', 'caixa', 'float', 'natal_acc_estrela_dourada.png', 0.62, 0.62, 0.55, false, 5),
 ('cartola_azevinho', 'Cartola de Azevinho', 'incomum', 'caixa', 'head', 'natal_acc_cartola_azevinho.png', 0, 0.55, 1.0, false, 6),
 ('laco_vermelho', 'Laco Vermelho', 'comum', 'caixa', 'head', 'natal_acc_laco_vermelho.png', 0, 0.55, 1.0, false, 7),
 ('guirlanda', 'Guirlanda', 'incomum', 'caixa', 'neck', 'natal_acc_guirlanda.png', 0, -0.5, 1.0, false, 8),
 ('cachecol_listrado', 'Cachecol Listrado', 'comum', 'caixa', 'neck', 'natal_acc_cachecol_listrado.png', 0, -0.5, 1.0, false, 9),
 ('cartola_floco', 'Cartola Floco de Neve', 'raro', 'caixa', 'head', 'natal_acc_cartola_floco.png', 0, 0.55, 1.0, false, 10),
 ('sinos_natal', 'Sinos de Natal', 'incomum', 'caixa', 'neck', 'natal_acc_sinos_natal.png', 0, -0.5, 1.0, false, 11),
 ('pisca_pisca', 'Pisca-Pisca', 'incomum', 'caixa', 'neck', 'natal_acc_pisca_pisca.png', 0, -0.5, 1.0, false, 12),
 ('laco_xadrez', 'Laco Xadrez', 'comum', 'caixa', 'head', 'natal_acc_laco_xadrez.png', 0, 0.55, 1.0, false, 13),
 ('boneco_gengibre', 'Boneco de Gengibre', 'raro', 'caixa', 'side', 'natal_acc_boneco_gengibre.png', -0.78, -0.2, 0.6, false, 14),
 ('meia_natal', 'Meia de Natal', 'incomum', 'caixa', 'side', 'natal_acc_meia_natal.png', -0.78, -0.2, 0.6, false, 15),
 ('presente', 'Presente', 'comum', 'caixa', 'side', 'natal_acc_presente.png', -0.78, -0.2, 0.6, false, 16),
 ('sino_dourado', 'Sino Dourado', 'incomum', 'caixa', 'neck', 'natal_acc_sino_dourado.png', 0, -0.5, 1.0, false, 17),
 ('coroa_bengala', 'Coroa de Bengala', 'raro', 'caixa', 'head', 'natal_acc_coroa_bengala.png', 0, 0.55, 1.0, false, 18),
 ('azevinho', 'Azevinho', 'comum', 'caixa', 'head', 'natal_acc_azevinho.png', 0, 0.55, 1.0, false, 19),
 ('floco_neve', 'Floco de Neve', 'comum', 'caixa', 'float', 'natal_acc_floco_neve.png', 0.62, 0.62, 0.55, false, 20),
 ('coroa_gelo', 'Coroa de Gelo', 'epico', 'caixa', 'head', 'natal_acc_coroa_gelo.png', 0, 0.55, 1.0, false, 21),
 ('aureola', 'Aureola', 'epico', 'passe', 'halo', 'natal_acc_aureola.png', 0, 0.82, 0.9, false, 22),
 ('asas_anjo', 'Asas de Anjo', 'epico', 'passe', 'back', 'natal_acc_asas_anjo.png', 0, 0.05, 1.7, true, 23),
 ('chifres_diabo', 'Chifres', 'raro', 'caixa', 'head', 'natal_acc_chifres_diabo.png', 0, 0.55, 1.0, false, 24),
 ('asas_morcego', 'Asas de Morcego', 'epico', 'caixa', 'back', 'natal_acc_asas_morcego.png', 0, 0.05, 1.7, true, 25),
 ('coroa_roxa', 'Coroa Roxa', 'raro', 'caixa', 'head', 'natal_acc_coroa_roxa.png', 0, 0.55, 1.0, false, 26),
 ('coroa_dourada', 'Coroa Dourada', 'lendario', 'caixa', 'head', 'natal_acc_coroa_dourada.png', 0, 0.55, 1.0, false, 27),
 ('coroa_doce', 'Coroa Doce', 'raro', 'caixa', 'head', 'natal_acc_coroa_doce.png', 0, 0.55, 1.0, false, 28),
 ('protetor_orelha', 'Protetor de Orelha', 'incomum', 'caixa', 'head', 'natal_acc_protetor_orelha.png', 0, 0.55, 1.0, false, 29),
 ('barba_noel', 'Barba do Noel', 'incomum', 'caixa', 'face', 'natal_acc_barba_noel.png', 0, -0.22, 0.85, false, 30),
 ('gravata_borboleta', 'Gravata Borboleta', 'comum', 'caixa', 'neck', 'natal_acc_gravata_borboleta.png', 0, -0.5, 1.0, false, 31),
 ('cachecol_xadrez', 'Cachecol Xadrez', 'incomum', 'caixa', 'neck', 'natal_acc_cachecol_xadrez.png', 0, -0.5, 1.0, false, 32),
 ('boneco_neve', 'Boneco de Neve', 'raro', 'calendario', 'side', 'natal_acc_boneco_neve.png', -0.78, -0.2, 0.6, false, 33),
 ('rena', 'Rena', 'raro', 'caixa', 'side', 'natal_acc_rena.png', -0.78, -0.2, 0.6, false, 34),
 ('estrela_cadente', 'Estrela Cadente', 'epico', 'calendario', 'float', 'natal_acc_estrela_cadente.png', 0.62, 0.62, 0.55, false, 35),
 ('coroa_gelo_real', 'Coroa de Gelo Real', 'lendario', 'passe', 'head', 'natal_acc_coroa_gelo_real.png', 0, 0.55, 1.0, false, 36),
 ('coroa_rosa', 'Coroa Rosa', 'epico', 'caixa', 'head', 'natal_acc_coroa_rosa.png', 0, 0.55, 1.0, false, 37),
 ('coroa_sombria', 'Coroa Sombria', 'lendario', 'caixa', 'head', 'natal_acc_coroa_sombria.png', 0, 0.55, 1.0, false, 38),
 ('coroa_infernal', 'Coroa Infernal', 'lendario', 'caixa', 'head', 'natal_acc_coroa_infernal.png', 0, 0.55, 1.0, false, 39),
 ('coroa_espinhos', 'Coroa de Espinhos', 'epico', 'caixa', 'head', 'natal_acc_coroa_espinhos.png', 0, 0.55, 1.0, false, 40),
 ('anel_saturno', 'Anel de Saturno', 'lendario', 'passe', 'float', 'natal_acc_anel_saturno.png', 0.62, 0.62, 0.55, false, 41),
 ('planeta_roxo', 'Planeta Roxo', 'extremamente_raro', 'caixa', 'float', 'natal_acc_planeta_roxo.png', 0.62, 0.62, 0.55, false, 42),
 ('coroa_abismo', 'Coroa do Abismo', 'extremamente_raro', 'caixa', 'head', 'natal_acc_coroa_abismo.png', 0, 0.55, 1.0, false, 43),
 ('louros_dourados', 'Louros Dourados', 'lendario', 'passe', 'head', 'natal_acc_louros_dourados.png', 0, 0.55, 1.0, false, 44),
 ('laco_sombrio', 'Laco Sombrio', 'extremamente_raro', 'caixa', 'head', 'natal_acc_laco_sombrio.png', 0, 0.55, 1.0, false, 45)
on conflict (id) do nothing;

-- Event levels and minigames (owner adds them like the Main Levels) ---------------
create table if not exists public.geobr_natal_levels (
 id bigint generated by default as identity primary key check (id between 1 and 1000000),
 kind text not null default 'fase' check (kind in ('fase','minigame')),
 sort_order int not null default 1,
 name text not null check (char_length(name) between 1 and 64),
 creator text not null default 'Equipe GeoBR' check (char_length(creator) between 1 and 40),
 description text not null default '' check (char_length(description) <= 300),
 difficulty text not null default 'Normal',
 stars smallint not null default 0 check (stars between 0 and 10),
 song text not null default '' check (char_length(song) <= 100),
 platformer boolean not null default false,
 candy_reward int not null default 50 check (candy_reward between 0 and 1000),
 min_seconds int not null default 20 check (min_seconds between 5 and 3600),
 gmd_path text not null unique check (char_length(gmd_path) between 5 and 240 and gmd_path !~ '(^/|\\|://)' and position('..' in gmd_path) = 0 and lower(gmd_path) ~ '\.(gmd|gmd2|lvl)$'),
 file_version int not null default 1 check (file_version between 1 and 1000000),
 published boolean not null default false,
 created_at timestamptz not null default now(),
 updated_at timestamptz not null default now()
);
alter table public.geobr_natal_levels enable row level security;
grant select on public.geobr_natal_levels to anon, authenticated;
grant insert, update, delete on public.geobr_natal_levels to authenticated;
do $$ begin
 if not exists (select 1 from pg_policies where tablename = 'geobr_natal_levels' and policyname = 'natal_levels_read') then
  create policy natal_levels_read on public.geobr_natal_levels for select to anon, authenticated using (published);
  create policy natal_levels_admin on public.geobr_natal_levels for all to authenticated using (geobr_private.is_admin()) with check (geobr_private.is_admin());
 end if;
end $$;

-- Players ---------------------------------------------------------------------------
create table if not exists geobr_private.natal_players (
 user_id uuid primary key references auth.users(id) on delete cascade,
 candy bigint not null default 0 check (candy >= 0),
 lifetime bigint not null default 0 check (lifetime >= 0),
 equipped text references public.geobr_natal_accessories(id) on delete set null,
 banned boolean not null default false,
 ban_reason text,
 updated_at timestamptz not null default now()
);
create table if not exists geobr_private.natal_inventory (
 user_id uuid not null references auth.users(id) on delete cascade,
 accessory_id text not null references public.geobr_natal_accessories(id) on delete cascade,
 source text not null,
 acquired_at timestamptz not null default now(),
 primary key (user_id, accessory_id)
);
create table if not exists geobr_private.natal_ledger (
 id bigint generated always as identity primary key,
 user_id uuid not null references auth.users(id) on delete cascade,
 delta bigint not null,
 reason text not null,
 ref text not null default '',
 created_at timestamptz not null default now(),
 unique (user_id, reason, ref)
);
create table if not exists geobr_private.natal_spins (user_id uuid not null references auth.users(id) on delete cascade, day date not null, reward jsonb not null, primary key (user_id, day));
create table if not exists geobr_private.natal_calendar_claims (user_id uuid not null references auth.users(id) on delete cascade, day int not null check (day between 1 and 25), reward jsonb not null, at timestamptz not null default now(), primary key (user_id, day));
create table if not exists geobr_private.natal_pass_claims (user_id uuid not null references auth.users(id) on delete cascade, tier int not null, at timestamptz not null default now(), primary key (user_id, tier));
create table if not exists geobr_private.natal_admin_log (id bigint generated always as identity primary key, admin uuid, action text not null, data jsonb, at timestamptz not null default now());

-- Battles (classic levels: best %, platformer minigames: fastest time) -----------
create table if not exists geobr_private.natal_battles (
 id uuid primary key default gen_random_uuid(),
 code text not null unique,
 host uuid not null references auth.users(id) on delete cascade,
 title text not null,
 level_id bigint not null references public.geobr_natal_levels(id) on delete cascade,
 mode text not null check (mode in ('classic','platformer')),
 state text not null default 'lobby' check (state in ('lobby','running','finished')),
 max_players int not null default 8 check (max_players between 2 and 16),
 starts_at timestamptz, ends_at timestamptz,
 finalized boolean not null default false,
 created_at timestamptz not null default now()
);
create table if not exists geobr_private.natal_battle_players (
 battle_id uuid not null references geobr_private.natal_battles(id) on delete cascade,
 user_id uuid not null references auth.users(id) on delete cascade,
 display_name text not null,
 percent int not null default 0 check (percent between 0 and 100),
 best_ms int,
 finished boolean not null default false,
 guard_ok boolean not null default false,
 guard_mods text[] not null default '{}',
 disqualified boolean not null default false,
 place int, prize int not null default 0,
 joined_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 primary key (battle_id, user_id)
);

do $$ declare t text; begin
 foreach t in array array['natal_config','natal_players','natal_inventory','natal_ledger','natal_spins','natal_calendar_claims','natal_pass_claims','natal_admin_log','natal_battles','natal_battle_players'] loop
  execute format('alter table geobr_private.%I enable row level security', t);
  execute format('revoke all on geobr_private.%I from public, anon, authenticated', t);
 end loop;
end $$;

-- Helpers -------------------------------------------------------------------------------
create or replace function geobr_private.natal_today() returns date language sql stable set search_path = '' as $$
 select (now() at time zone 'America/Sao_Paulo')::date $$;

create or replace function geobr_private.natal_active() returns boolean language sql stable security definer set search_path = '' as $$
 select coalesce((select enabled and now() >= starts_at and now() < ends_at from geobr_private.natal_config where id = 1), false) $$;

-- The caller's player row (created on first use); refuses banned players.
create or replace function geobr_private.natal_me(p_need_active boolean default true) returns geobr_private.natal_players
language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players;
begin
 perform geobr_private.require_access();
 insert into geobr_private.natal_players(user_id) values (auth.uid()) on conflict do nothing;
 select * into p from geobr_private.natal_players where user_id = auth.uid() for update;
 if p.banned then raise exception 'Voce esta suspenso do evento de Natal: %', coalesce(p.ban_reason, 'fale com a staff'); end if;
 if p_need_active and not geobr_private.natal_active() then raise exception 'O evento de Natal nao esta aberto agora'; end if;
 return p;
end $$;

create or replace function geobr_private.natal_add_candy(p_user uuid, p_amount bigint, p_reason text, p_ref text) returns boolean
language plpgsql security definer set search_path = '' as $$
declare n int;
begin
 insert into geobr_private.natal_ledger(user_id, delta, reason, ref) values (p_user, p_amount, p_reason, coalesce(p_ref, '')) on conflict do nothing;
 get diagnostics n = row_count;
 if n = 0 then return false; end if;
 insert into geobr_private.natal_players(user_id) values (p_user) on conflict do nothing;
 update geobr_private.natal_players set candy = greatest(0, candy + p_amount), lifetime = lifetime + greatest(0, p_amount), updated_at = now() where user_id = p_user;
 return true;
end $$;

-- Random accessory of a rarity from the boxes; a repeated one becomes Candy.
create or replace function geobr_private.natal_open(p_user uuid, p_rarity text, p_ref text) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare a public.geobr_natal_accessories; refund int; owned boolean;
begin
 select * into a from public.geobr_natal_accessories where rarity = p_rarity and source = 'caixa' and published order by random() limit 1;
 if a.id is null then raise exception 'Nenhum acessorio % disponivel', p_rarity; end if;
 owned = exists (select 1 from geobr_private.natal_inventory where user_id = p_user and accessory_id = a.id);
 if owned then
  refund = coalesce(((select duplicate_refund from geobr_private.natal_config where id = 1) ->> p_rarity)::int, 10);
  perform geobr_private.natal_add_candy(p_user, refund, 'duplicado', p_ref);
  return jsonb_build_object('kind', 'accessory', 'id', a.id, 'name', a.name, 'rarity', a.rarity, 'duplicate', true, 'refund', refund);
 end if;
 insert into geobr_private.natal_inventory(user_id, accessory_id, source) values (p_user, a.id, 'caixa');
 return jsonb_build_object('kind', 'accessory', 'id', a.id, 'name', a.name, 'rarity', a.rarity, 'duplicate', false);
end $$;

-- Gives one reward ({kind: candy|box|accessory}).
create or replace function geobr_private.natal_give(p_user uuid, p_reward jsonb, p_reason text, p_ref text) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare kind text = p_reward ->> 'kind'; a public.geobr_natal_accessories;
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
 end if;
 raise exception 'Premio invalido';
end $$;

-- Everything the Natal screen shows.
create or replace function geobr_private.natal_state() returns jsonb language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; p geobr_private.natal_players; today date = geobr_private.natal_today();
begin
 perform geobr_private.require_access();
 select * into c from geobr_private.natal_config where id = 1;
 insert into geobr_private.natal_players(user_id) values (auth.uid()) on conflict do nothing;
 select * into p from geobr_private.natal_players where user_id = auth.uid();
 return jsonb_build_object(
  'event', jsonb_build_object('enabled', c.enabled, 'active', geobr_private.natal_active(), 'starts_at', c.starts_at, 'ends_at', c.ends_at,
    'today', today, 'calendar_day', case when extract(month from today) = 12 then least(25, extract(day from today))::int else 0 end,
    'calendar_catchup', c.calendar_catchup),
  'me', jsonb_build_object('candy', p.candy, 'lifetime', p.lifetime, 'equipped', p.equipped, 'banned', p.banned, 'ban_reason', p.ban_reason),
  'is_admin', geobr_private.is_admin(),
  'inventory', coalesce((select jsonb_agg(accessory_id order by acquired_at) from geobr_private.natal_inventory where user_id = auth.uid()), '[]'::jsonb),
  'spin_today', (select reward from geobr_private.natal_spins where user_id = auth.uid() and day = today),
  'calendar_claimed', coalesce((select jsonb_agg(day order by day) from geobr_private.natal_calendar_claims where user_id = auth.uid()), '[]'::jsonb),
  'calendar', c.calendar, 'roulette', c.roulette, 'box_prices', c.box_prices, 'pass', c.pass,
  'pass_claimed', coalesce((select jsonb_agg(tier order by tier) from geobr_private.natal_pass_claims where user_id = auth.uid()), '[]'::jsonb),
  'levels_done', coalesce((select jsonb_agg(ref::bigint) from geobr_private.natal_ledger where user_id = auth.uid() and reason = 'fase'), '[]'::jsonb));
end $$;

create or replace function geobr_private.natal_spin() returns jsonb language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; total int; pick int; seg jsonb; acc int = 0; result jsonb; today date = geobr_private.natal_today(); idx int = 0;
begin
 perform geobr_private.natal_me();
 if exists (select 1 from geobr_private.natal_spins where user_id = auth.uid() and day = today) then raise exception 'Voce ja girou a roleta hoje'; end if;
 select * into c from geobr_private.natal_config where id = 1;
 select sum((s ->> 'weight')::int) into total from jsonb_array_elements(c.roulette) s;
 pick = floor(random() * total)::int;
 for seg in select * from jsonb_array_elements(c.roulette) loop
  acc = acc + (seg ->> 'weight')::int;
  exit when pick < acc;
  idx = idx + 1;
 end loop;
 result = geobr_private.natal_give(auth.uid(), seg - 'weight', 'roleta', today::text) || jsonb_build_object('segment', idx);
 insert into geobr_private.natal_spins(user_id, day, reward) values (auth.uid(), today, result);
 return jsonb_build_object('reward', result, 'state', geobr_private.natal_state());
end $$;

create or replace function geobr_private.natal_calendar_claim(p_day int) returns jsonb language plpgsql security definer set search_path = '' as $$
declare c geobr_private.natal_config; today date = geobr_private.natal_today(); open_day int; result jsonb;
begin
 perform geobr_private.natal_me();
 select * into c from geobr_private.natal_config where id = 1;
 open_day = case when extract(month from today) = 12 then least(25, extract(day from today))::int else 0 end;
 if p_day is null or p_day < 1 or p_day > 25 or p_day > open_day then raise exception 'Esse dia ainda nao abriu'; end if;
 if not c.calendar_catchup and p_day <> open_day then raise exception 'So da para abrir o dia de hoje'; end if;
 if exists (select 1 from geobr_private.natal_calendar_claims where user_id = auth.uid() and day = p_day) then raise exception 'Dia ja aberto'; end if;
 result = geobr_private.natal_give(auth.uid(), c.calendar -> (p_day - 1), 'calendario', p_day::text);
 insert into geobr_private.natal_calendar_claims(user_id, day, reward) values (auth.uid(), p_day, result);
 return jsonb_build_object('reward', result, 'state', geobr_private.natal_state());
end $$;

create or replace function geobr_private.natal_open_box(p_rarity text) returns jsonb language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players; price int; result jsonb; ref text = gen_random_uuid()::text;
begin
 p = geobr_private.natal_me();
 price = ((select box_prices from geobr_private.natal_config where id = 1) ->> p_rarity)::int;
 if price is null then raise exception 'Caixa invalida'; end if;
 if p.candy < price then raise exception 'Candy insuficiente (precisa de %)', price; end if;
 perform geobr_private.natal_add_candy(auth.uid(), -price, 'caixa', ref);
 result = geobr_private.natal_open(auth.uid(), p_rarity, ref) || jsonb_build_object('box', p_rarity, 'price', price);
 return jsonb_build_object('reward', result, 'state', geobr_private.natal_state());
end $$;

create or replace function geobr_private.natal_equip(p_accessory text) returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 perform geobr_private.natal_me(false);
 if p_accessory is not null and not exists (select 1 from geobr_private.natal_inventory where user_id = auth.uid() and accessory_id = p_accessory) then
  raise exception 'Voce nao tem esse acessorio';
 end if;
 update geobr_private.natal_players set equipped = p_accessory, updated_at = now() where user_id = auth.uid();
 return geobr_private.natal_state();
end $$;

create or replace function geobr_private.natal_pass_claim(p_tier int) returns jsonb language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players; t jsonb; result jsonb;
begin
 p = geobr_private.natal_me();
 t = (select pass from geobr_private.natal_config where id = 1) -> (p_tier - 1);
 if t is null then raise exception 'Nivel do passe invalido'; end if;
 if p.lifetime < (t ->> 'need')::int then raise exception 'Junte % Candy no evento para liberar', (t ->> 'need')::int; end if;
 insert into geobr_private.natal_pass_claims(user_id, tier) values (auth.uid(), p_tier) on conflict do nothing;
 if not found then raise exception 'Premio ja resgatado'; end if;
 result = geobr_private.natal_give(auth.uid(), t -> 'reward', 'passe', p_tier::text);
 return jsonb_build_object('reward', result, 'state', geobr_private.natal_state());
end $$;

-- Completing an event level once gives its Candy (minimum play time checked).
create or replace function geobr_private.natal_level_complete(p_level bigint, p_elapsed real) returns jsonb language plpgsql security definer set search_path = '' as $$
declare l public.geobr_natal_levels; given boolean;
begin
 perform geobr_private.natal_me();
 select * into l from public.geobr_natal_levels where id = p_level and published;
 if l.id is null then raise exception 'Fase do evento nao encontrada'; end if;
 if coalesce(p_elapsed, 0) < l.min_seconds then return jsonb_build_object('ok', false, 'reason', 'tempo'); end if;
 given = l.candy_reward > 0 and geobr_private.natal_add_candy(auth.uid(), l.candy_reward, 'fase', l.id::text);
 return jsonb_build_object('ok', true, 'candy', case when given then l.candy_reward else 0 end, 'state', geobr_private.natal_state());
end $$;

-- Equipped accessory of GDPS accounts (profiles, battle list).
create or replace function geobr_private.natal_equipped(p_accounts bigint[]) returns jsonb language sql stable security definer set search_path = '' as $$
 select coalesce(jsonb_agg(jsonb_build_object('account_id', a.account_id, 'accessory', p.equipped)), '[]'::jsonb)
 from geobr_gdps.accounts a join geobr_private.natal_players p on p.user_id = a.supabase_user
 join public.geobr_natal_accessories x on x.id = p.equipped and x.published
 where a.account_id = any(coalesce(p_accounts, '{}')) and not p.banned
$$;

-- Owner tools (GeoBR admin session), every action logged -----------------------------
create or replace function geobr_private.natal_admin(p_action text, p_data jsonb) returns jsonb language plpgsql security definer set search_path = '' as $$
declare target uuid; result jsonb = '{}'::jsonb; d jsonb = coalesce(p_data, '{}'::jsonb);
begin
 if not geobr_private.is_admin() then raise exception 'Entre no painel do dono primeiro'; end if;
 if d ? 'player' then
  select supabase_user into target from geobr_gdps.accounts where lower(username) = lower(trim(d ->> 'player')) limit 1;
  if target is null then raise exception 'Jogador nao encontrado (ou sem conta GeoBR vinculada)'; end if;
 end if;
 if p_action = 'config' then
  update geobr_private.natal_config set
   enabled = coalesce((d ->> 'enabled')::boolean, enabled),
   starts_at = coalesce((d ->> 'starts_at')::timestamptz, starts_at),
   ends_at = coalesce((d ->> 'ends_at')::timestamptz, ends_at),
   calendar_catchup = coalesce((d ->> 'calendar_catchup')::boolean, calendar_catchup),
   box_prices = coalesce(d -> 'box_prices', box_prices), duplicate_refund = coalesce(d -> 'duplicate_refund', duplicate_refund),
   roulette = coalesce(d -> 'roulette', roulette), calendar = coalesce(d -> 'calendar', calendar), pass = coalesce(d -> 'pass', pass),
   battle_prizes = coalesce(d -> 'battle_prizes', battle_prizes), battle_seconds = coalesce(d -> 'battle_seconds', battle_seconds),
   guard_mods = coalesce((select array_agg(x) from jsonb_array_elements_text(d -> 'guard_mods') x), guard_mods),
   updated_at = now(), updated_by = auth.uid() where id = 1;
  select to_jsonb(c) into result from geobr_private.natal_config c where id = 1;
 elsif p_action = 'get_config' then
  select to_jsonb(c) into result from geobr_private.natal_config c where id = 1;
 elsif p_action = 'grant' then
  if target is null then raise exception 'Informe o jogador'; end if;
  perform geobr_private.natal_add_candy(target, (d ->> 'amount')::bigint, 'dono', gen_random_uuid()::text);
  select jsonb_build_object('candy', candy) into result from geobr_private.natal_players where user_id = target;
 elsif p_action = 'ban' then
  if target is null then raise exception 'Informe o jogador'; end if;
  insert into geobr_private.natal_players(user_id) values (target) on conflict do nothing;
  update geobr_private.natal_players set banned = coalesce((d ->> 'banned')::boolean, true), ban_reason = left(d ->> 'reason', 120), updated_at = now() where user_id = target;
  update geobr_private.natal_battle_players p set disqualified = true from geobr_private.natal_battles b
   where p.battle_id = b.id and b.state <> 'finished' and p.user_id = target and coalesce((d ->> 'banned')::boolean, true);
  result = jsonb_build_object('banned', coalesce((d ->> 'banned')::boolean, true));
 elsif p_action = 'player' then
  if target is null then raise exception 'Informe o jogador'; end if;
  select jsonb_build_object('candy', p.candy, 'lifetime', p.lifetime, 'banned', p.banned, 'reason', p.ban_reason, 'equipped', p.equipped,
   'accessories', (select count(*) from geobr_private.natal_inventory i where i.user_id = target),
   'recent', (select jsonb_agg(jsonb_build_object('delta', l.delta, 'reason', l.reason, 'at', l.created_at) order by l.id desc)
     from (select * from geobr_private.natal_ledger where user_id = target order by id desc limit 8) l))
  into result from geobr_private.natal_players p where p.user_id = target;
 elsif p_action = 'close_battle' then
  update geobr_private.natal_battles set state = 'finished', finalized = true where code = upper(d ->> 'code');
 else
  raise exception 'Acao desconhecida';
 end if;
 insert into geobr_private.natal_admin_log(admin, action, data) values (auth.uid(), p_action, d);
 return coalesce(result, '{}'::jsonb);
end $$;

-- Public RPC wrappers (authenticated players only) ----------------------------------------
do $$
declare f record;
begin
 for f in select * from (values
  ('natal_state', ''), ('natal_spin', ''), ('natal_calendar_claim', 'p_day int'), ('natal_open_box', 'p_rarity text'),
  ('natal_equip', 'p_accessory text'), ('natal_pass_claim', 'p_tier int'), ('natal_level_complete', 'p_level bigint, p_elapsed real'),
  ('natal_equipped', 'p_accounts bigint[]'), ('natal_admin', 'p_action text, p_data jsonb')) as t(name, sig)
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
 foreach f in array array['natal_today()','natal_active()','natal_me(boolean)','natal_add_candy(uuid,bigint,text,text)','natal_open(uuid,text,text)',
  'natal_give(uuid,jsonb,text,text)'] loop
  execute format('revoke all on function geobr_private.%s from public, anon, authenticated', f);
 end loop;
end $$;

-- Test builds of the event (the release itself is the owner's decision).
insert into geobr_private.releases(version, enabled, message)
values ('1.16.0', true, 'GeoBR 1.16.0: GeoBR Natal 2026')
on conflict (version) do nothing;
