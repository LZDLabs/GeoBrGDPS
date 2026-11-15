-- GeoBR GDPS v2 (1.10.0-beta.2).
-- * Each server now has its own GD save on the device (companion mod geobr.gdps),
--   so GDPS IDs no longer need an offset: accounts and levels count from 1 again.
-- * Hides the data of the 2026-11-08 live test (accounts GeoBRTeste*).
-- * Adds what a full GDPS has: map packs, gauntlets, level lists, chests and
--   quests served by the GDPS, comment mutes.
-- Same lock-down as v1: schema geobr_gdps, RLS on, no API grants.

-- -------------------------------------------- live-test data (2026-11-08)
-- Hidden, not deleted (no destructive statements): the two GeoBRTeste accounts
-- are banned (out of searches, rankings and logins) and their level is marked
-- deleted. Their old IDs (900000001+) never collide with the new numbering.
update geobr_gdps.accounts set is_banned = true, ban_reason = 'conta de teste', banned_until = null,
  lb_banned = true, lb_ban_reason = 'conta de teste'
where username like 'GeoBRTeste%' and account_id >= 900000001;
update geobr_gdps.levels set deleted = true
where account_id in (select account_id from geobr_gdps.accounts where username like 'GeoBRTeste%') and not deleted;

-- -------------------------------------------------- IDs count from 1 again
-- Next ID = 1 + the highest ID below the old 900000001 offset.
do $$
declare next_account bigint; next_level bigint;
begin
 select coalesce(max(account_id), 0) + 1 into next_account from geobr_gdps.accounts where account_id < 900000001;
 select coalesce(max(level_id), 0) + 1 into next_level from geobr_gdps.levels where level_id < 900000001;
 if not exists (select 1 from geobr_gdps.accounts where account_id >= 900000001 and username not like 'GeoBRTeste%') then
  execute format('alter table geobr_gdps.accounts alter column account_id restart with %s', next_account);
 end if;
 if not exists (select 1 from geobr_gdps.levels l join geobr_gdps.accounts a using (account_id) where l.level_id >= 900000001 and a.username not like 'GeoBRTeste%') then
  execute format('alter table geobr_gdps.levels alter column level_id restart with %s', next_level);
 end if;
end $$;

-- ------------------------------------------------------- comment mutes
alter table geobr_gdps.accounts add column if not exists comment_ban_until timestamptz;
alter table geobr_gdps.accounts add column if not exists comment_ban_reason text;

-- ----------------------------------------------------------- map packs
create table if not exists geobr_gdps.map_packs (
 pack_id bigint generated always as identity primary key,
 name text not null check (char_length(name) between 1 and 40),
 level_ids text not null,                    -- "1,2,3" (GDPS level IDs, in order)
 stars int not null default 0 check (stars between 0 and 50),
 coins int not null default 0 check (coins between 0 and 10),
 difficulty int not null default 0 check (difficulty between 0 and 10),
 text_color text not null default '255,255,255',
 bar_color text not null default '255,255,255',
 position int not null default 100,
 created_by bigint references geobr_gdps.accounts(account_id) on delete set null,
 created_at timestamptz not null default now()
);
create index if not exists gdps_map_packs_position_idx on geobr_gdps.map_packs (position, pack_id);
create index if not exists gdps_map_packs_created_by_idx on geobr_gdps.map_packs (created_by);

-- ----------------------------------------------------------- gauntlets
-- The ID picks GD's gauntlet theme (1 Fire, 2 Ice, 3 Poison, 4 Shadow, 5 Lava...).
create table if not exists geobr_gdps.gauntlets (
 gauntlet_id int primary key check (gauntlet_id between 1 and 99),
 level_ids text not null,                    -- exactly 5 level IDs
 updated_by bigint references geobr_gdps.accounts(account_id) on delete set null,
 updated_at timestamptz not null default now()
);
create index if not exists gdps_gauntlets_updated_by_idx on geobr_gdps.gauntlets (updated_by);

-- --------------------------------------------------------- level lists
create table if not exists geobr_gdps.level_lists (
 list_id bigint generated always as identity primary key,
 account_id bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 name text not null check (char_length(name) between 1 and 40),
 description text not null default '',
 level_ids text not null default '',
 difficulty int not null default -1,
 version int not null default 1,
 original bigint not null default 0,
 unlisted int not null default 0,
 downloads int not null default 0,
 likes int not null default 0,
 stars int not null default 0,
 featured int not null default 0,
 count_for_reward int not null default 0,
 created_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 deleted boolean not null default false
);
create index if not exists gdps_level_lists_account_idx on geobr_gdps.level_lists (account_id);
create index if not exists gdps_level_lists_recent_idx on geobr_gdps.level_lists (list_id desc) where not deleted;

-- --------------------------------------------------- chests and quests
create table if not exists geobr_gdps.chest_state (
 subject text primary key,                   -- 'a:<accountID>' or 'u:<udid>'
 small_at timestamptz,
 small_count int not null default 0,
 big_at timestamptz,
 big_count int not null default 0
);

create table if not exists geobr_gdps.quests (
 quest_id bigint generated always as identity primary key,
 kind int not null check (kind in (1, 2, 3)),  -- 1 orbs, 2 coins, 3 stars
 amount int not null check (amount > 0),
 reward int not null check (reward between 1 and 100),  -- diamonds
 name text not null check (char_length(name) between 1 and 40),
 enabled boolean not null default true
);
insert into geobr_gdps.quests (kind, amount, reward, name)
select * from (values
 (1, 200, 10, 'Coletor de Orbs'), (1, 500, 15, 'Mestre dos Orbs'), (1, 1000, 20, 'Orbs sem fim'),
 (2, 2, 10, 'Caca-moedas'), (2, 4, 15, 'Moedas de prata'), (2, 6, 20, 'Tesouro GeoBR'),
 (3, 5, 10, 'Primeiras estrelas'), (3, 10, 15, 'Estrelado'), (3, 20, 20, 'Constelacao')
) as q(kind, amount, reward, name)
where not exists (select 1 from geobr_gdps.quests);

-- Map Packs and Gauntlets are curated by a new permission.
update geobr_gdps.roles set permissions = array_append(permissions, 'packs.manage')
where name in ('Admin', 'Event Manager') and not ('packs.manage' = any(permissions));

-- ------------------------------------------------------- lock everything
do $$
declare t record;
begin
 for t in select tablename from pg_tables where schemaname = 'geobr_gdps' loop
  execute format('alter table geobr_gdps.%I enable row level security', t.tablename);
  execute format('revoke all on geobr_gdps.%I from public', t.tablename);
  if exists (select 1 from pg_roles where rolname = 'anon') then execute format('revoke all on geobr_gdps.%I from anon', t.tablename); end if;
  if exists (select 1 from pg_roles where rolname = 'authenticated') then execute format('revoke all on geobr_gdps.%I from authenticated', t.tablename); end if;
 end loop;
end $$;

insert into geobr_private.releases(version, enabled, message)
values ('1.10.0-beta.2', true, 'GeoBR GDPS v2')
on conflict (version) do nothing;
