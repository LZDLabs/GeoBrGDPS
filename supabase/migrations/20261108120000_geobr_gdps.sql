-- GeoBR GDPS (1.10.0-beta.1) — private server data for the `gdps` Edge Function.
-- Forward-only, additive: nothing existing is changed or removed.
--
-- Everything lives in schema geobr_gdps, which the Data API does not expose.
-- RLS is enabled on every table with NO policies and API roles have no
-- grants, so only the Edge Function (server-side database connection)
-- reads or writes it. Permissions are checked in the function, per request,
-- against the roles stored here; every moderation action is written to
-- geobr_gdps.audit_log.

create schema if not exists geobr_gdps;
revoke all on schema geobr_gdps from public;
do $$ begin
 if exists (select 1 from pg_roles where rolname = 'anon') then execute 'revoke all on schema geobr_gdps from anon'; end if;
 if exists (select 1 from pg_roles where rolname = 'authenticated') then execute 'revoke all on schema geobr_gdps from authenticated'; end if;
end $$;

-- ------------------------------------------------------------- accounts
create table if not exists geobr_gdps.accounts (
 -- IDs start far above RobTop's so GD's local caches never mix the two servers.
 account_id bigint generated always as identity (start with 900000001) primary key,
 username text not null check (username ~ '^[A-Za-z0-9]{3,20}$'),
 gjp2_hash text not null,                 -- bcrypt of GD's gjp2 (sha1 of password + salt); no email is stored
 created_at timestamptz not null default now(),
 last_login timestamptz,
 is_banned boolean not null default false,
 ban_reason text,
 banned_until timestamptz,
 lb_banned boolean not null default false,  -- leaderboard-only ban
 lb_ban_reason text,
 recovery_hash text,                      -- bcrypt of the recovery code
 recovery_created_at timestamptz,
 supabase_user uuid unique references auth.users(id) on delete set null,
 stars int not null default 0, moons int not null default 0, demons int not null default 0,
 diamonds int not null default 0, coins int not null default 0, user_coins int not null default 0,
 creator_points int not null default 0,
 icon int not null default 1, icon_type int not null default 0,
 color1 int not null default 0, color2 int not null default 3, color3 int not null default -1, special int not null default 0,
 acc_icon int not null default 1, acc_ship int not null default 1, acc_ball int not null default 1, acc_bird int not null default 1,
 acc_dart int not null default 1, acc_robot int not null default 1, acc_spider int not null default 1, acc_swing int not null default 1,
 acc_jetpack int not null default 1, acc_explosion int not null default 1, acc_glow int not null default 0,
 demons_info text not null default '', levels_info text not null default '', platformer_info text not null default '',
 msg_state int not null default 0, friend_state int not null default 0, comment_state int not null default 0,
 youtube text not null default '', twitter text not null default '', twitch text not null default '',
 stats_updated_at timestamptz
);
create unique index if not exists gdps_accounts_username_idx on geobr_gdps.accounts (lower(username));
create index if not exists gdps_accounts_stars_idx on geobr_gdps.accounts (stars desc) where not lb_banned and not is_banned;
create index if not exists gdps_accounts_cp_idx on geobr_gdps.accounts (creator_points desc) where creator_points > 0;

create table if not exists geobr_gdps.auth_attempts (
 id bigint generated always as identity primary key,
 kind text not null,          -- login | register | recovery | owner
 subject text not null,       -- lower(username) / ip / account id
 ok boolean not null,
 at timestamptz not null default now()
);
create index if not exists gdps_auth_attempts_idx on geobr_gdps.auth_attempts (kind, subject, at desc);

-- Every leaderboard update is kept, so the Leaderboard Moderator can see how a
-- player's numbers grew; big jumps are flagged.
create table if not exists geobr_gdps.stat_history (
 id bigint generated always as identity primary key,
 account_id bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 stars int not null, moons int not null, demons int not null, diamonds int not null,
 coins int not null, user_coins int not null,
 flagged boolean not null default false,
 at timestamptz not null default now()
);
create index if not exists gdps_stat_history_idx on geobr_gdps.stat_history (account_id, at desc);
create index if not exists gdps_stat_history_flagged_idx on geobr_gdps.stat_history (at desc) where flagged;

-- Cloud save (GD's own "Save"/"Load" in the account screen): the data goes to
-- the private Storage bucket geobr-gdps-saves; this row is its index.
create table if not exists geobr_gdps.saves (
 account_id bigint primary key references geobr_gdps.accounts(account_id) on delete cascade,
 object_path text not null,
 size_bytes bigint not null,
 game_version int not null default 22,
 binary_version int not null default 42,
 updated_at timestamptz not null default now()
);

-- --------------------------------------------------------------- levels
create table if not exists geobr_gdps.levels (
 level_id bigint generated always as identity (start with 900000001) primary key,
 account_id bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 name text not null check (char_length(name) between 1 and 20),
 description text not null default '',      -- base64 as GD sends it
 version int not null default 1,
 length int not null default 0,
 audio_track int not null default 0,
 song_id bigint not null default 0,
 song_ids text not null default '',
 sfx_ids text not null default '',
 game_version int not null default 22,
 binary_version int not null default 42,
 objects int not null default 0,
 coins int not null default 0,
 requested_stars int not null default 0,
 two_player boolean not null default false,
 original bigint not null default 0,
 password text not null default '0',
 unlisted int not null default 0,            -- 0 public, 1 unlisted, 2 friends only
 ldm boolean not null default false,
 wt int not null default 0, wt2 int not null default 0, ts int not null default 0,
 extra_string text not null default '',
 is_auto boolean not null default false,
 -- rating
 stars int not null default 0,
 difficulty int not null default 0,          -- 0 N/A, 10 easy .. 50 insane (demons use 50)
 demon boolean not null default false,
 demon_diff int not null default 0,          -- GD key 43: 3 easy, 4 medium, 0 hard, 5 insane, 6 extreme
 feature_score int not null default 0,       -- > 0 = featured
 epic int not null default 0,                -- 0 none, 1 epic, 2 legendary, 3 mythic
 coins_verified boolean not null default false,
 rated_at timestamptz,
 rated_by bigint references geobr_gdps.accounts(account_id) on delete set null,
 -- counters / moderation
 downloads int not null default 0,
 likes int not null default 0,
 created_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 deleted boolean not null default false,
 blocked boolean not null default false,     -- hidden by moderation
 block_reason text
);
create index if not exists gdps_levels_recent_idx on geobr_gdps.levels (level_id desc) where not deleted and not blocked;
create index if not exists gdps_levels_account_idx on geobr_gdps.levels (account_id);
create index if not exists gdps_levels_rated_idx on geobr_gdps.levels (rated_at desc) where stars > 0;
create index if not exists gdps_levels_name_idx on geobr_gdps.levels (lower(name));

create table if not exists geobr_gdps.level_data (
 level_id bigint primary key references geobr_gdps.levels(level_id) on delete cascade,
 data text not null check (octet_length(data) <= 16777216)
);

create table if not exists geobr_gdps.level_reports (
 id bigint generated always as identity primary key,
 level_id bigint not null references geobr_gdps.levels(level_id) on delete cascade,
 reporter text not null,
 at timestamptz not null default now()
);
create index if not exists gdps_level_reports_idx on geobr_gdps.level_reports (level_id);

create table if not exists geobr_gdps.rate_votes (
 level_id bigint not null references geobr_gdps.levels(level_id) on delete cascade,
 account_id bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 stars int not null,
 at timestamptz not null default now(),
 primary key (level_id, account_id)
);
create index if not exists gdps_rate_votes_account_idx on geobr_gdps.rate_votes (account_id);

create table if not exists geobr_gdps.suggestions (
 id bigint generated always as identity primary key,
 level_id bigint not null references geobr_gdps.levels(level_id) on delete cascade,
 account_id bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 stars int not null,
 feature boolean not null default false,
 at timestamptz not null default now()
);
create index if not exists gdps_suggestions_level_idx on geobr_gdps.suggestions (level_id);
create index if not exists gdps_suggestions_account_idx on geobr_gdps.suggestions (account_id);

-- Likes on levels (1), level comments (2), profile comments (3).
create table if not exists geobr_gdps.likes (
 item_type int not null check (item_type between 1 and 4),
 item_id bigint not null,
 voter text not null,                         -- 'a:<accountID>' or 'u:<udid>'
 value int not null check (value in (-1, 1)),
 at timestamptz not null default now(),
 primary key (item_type, item_id, voter)
);

-- ------------------------------------------------------------- comments
create table if not exists geobr_gdps.comments (
 comment_id bigint generated always as identity primary key,
 level_id bigint not null references geobr_gdps.levels(level_id) on delete cascade,
 account_id bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 body text not null,                          -- base64 as GD sends it
 percent int not null default 0,
 likes int not null default 0,
 created_at timestamptz not null default now(),
 deleted boolean not null default false
);
create index if not exists gdps_comments_level_idx on geobr_gdps.comments (level_id, comment_id desc) where not deleted;
create index if not exists gdps_comments_account_idx on geobr_gdps.comments (account_id, comment_id desc);

create table if not exists geobr_gdps.account_comments (
 comment_id bigint generated always as identity primary key,
 account_id bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 body text not null,
 likes int not null default 0,
 created_at timestamptz not null default now(),
 deleted boolean not null default false
);
create index if not exists gdps_account_comments_idx on geobr_gdps.account_comments (account_id, comment_id desc) where not deleted;

-- -------------------------------------------------------------- friends
create table if not exists geobr_gdps.friend_requests (
 request_id bigint generated always as identity primary key,
 from_account bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 to_account bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 body text not null default '',
 is_new boolean not null default true,
 created_at timestamptz not null default now(),
 unique (from_account, to_account)
);
create index if not exists gdps_friend_requests_to_idx on geobr_gdps.friend_requests (to_account);

create table if not exists geobr_gdps.friendships (
 a bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 b bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 new_for_a boolean not null default true,
 new_for_b boolean not null default true,
 created_at timestamptz not null default now(),
 primary key (a, b),
 check (a < b)
);
create index if not exists gdps_friendships_b_idx on geobr_gdps.friendships (b);

create table if not exists geobr_gdps.blocks (
 owner bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 target bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 created_at timestamptz not null default now(),
 primary key (owner, target)
);
create index if not exists gdps_blocks_target_idx on geobr_gdps.blocks (target);

create table if not exists geobr_gdps.messages (
 message_id bigint generated always as identity primary key,
 from_account bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 to_account bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 subject text not null,                       -- base64 as GD sends it
 body text not null,                          -- GD's XOR+base64 as sent
 is_read boolean not null default false,
 created_at timestamptz not null default now(),
 deleted_by_sender boolean not null default false,
 deleted_by_receiver boolean not null default false
);
create index if not exists gdps_messages_to_idx on geobr_gdps.messages (to_account, message_id desc);
create index if not exists gdps_messages_from_idx on geobr_gdps.messages (from_account, message_id desc);

-- -------------------------------------------------------------- scores
create table if not exists geobr_gdps.level_scores (
 level_id bigint not null references geobr_gdps.levels(level_id) on delete cascade,
 account_id bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 percent int not null default 0,
 attempts int not null default 0,
 clicks int not null default 0,
 seconds int not null default 0,
 coins int not null default 0,
 plat_time int not null default 0,            -- platformer: best time (ms), lower is better
 plat_points int not null default 0,
 updated_at timestamptz not null default now(),
 primary key (level_id, account_id)
);
create index if not exists gdps_level_scores_account_idx on geobr_gdps.level_scores (account_id);

-- ------------------------------------------------- daily / weekly / event
create table if not exists geobr_gdps.timely (
 timely_id bigint generated always as identity primary key,
 kind int not null check (kind in (0, 1, 2)),  -- 0 daily, 1 weekly, 2 event
 level_id bigint not null references geobr_gdps.levels(level_id) on delete cascade,
 starts_at timestamptz not null,
 created_by bigint references geobr_gdps.accounts(account_id) on delete set null,
 created_at timestamptz not null default now()
);
create index if not exists gdps_timely_kind_idx on geobr_gdps.timely (kind, starts_at desc);
create index if not exists gdps_timely_level_idx on geobr_gdps.timely (level_id);
create index if not exists gdps_timely_created_by_idx on geobr_gdps.timely (created_by);

-- ----------------------------------------------------------- demon list
create table if not exists geobr_gdps.demon_list (
 level_id bigint primary key references geobr_gdps.levels(level_id) on delete cascade,
 position int not null check (position >= 1),
 verifier text not null default '',
 added_by bigint references geobr_gdps.accounts(account_id) on delete set null,
 added_at timestamptz not null default now()
);
create index if not exists gdps_demon_list_position_idx on geobr_gdps.demon_list (position);
create index if not exists gdps_demon_list_added_by_idx on geobr_gdps.demon_list (added_by);

-- ---------------------------------------------------------- music library
-- Own, permanent IDs: 8000001..9999999. Above the Newgrounds range in use and
-- below GD's music-library range (10000000+), so GD treats them as custom
-- songs. The sequence never cycles, so an ID is never reused or duplicated.
create sequence if not exists geobr_gdps.song_id_seq start with 8000001 minvalue 8000001 maxvalue 9999999 no cycle;
create table if not exists geobr_gdps.songs (
 song_id bigint primary key default nextval('geobr_gdps.song_id_seq') check (song_id between 8000001 and 9999999),
 name text not null check (char_length(name) between 1 and 60),
 artist text not null check (char_length(artist) between 1 and 40),
 uploader bigint references geobr_gdps.accounts(account_id) on delete set null,
 object_path text not null unique,
 size_bytes bigint not null default 0,
 sha256 text,                                 -- content hash: the same MP3 is never stored twice
 status text not null default 'pending' check (status in ('uploading', 'pending', 'approved', 'blocked', 'removed')),
 created_at timestamptz not null default now(),
 published_at timestamptz,
 reviewed_by bigint references geobr_gdps.accounts(account_id) on delete set null
);
alter sequence geobr_gdps.song_id_seq owned by geobr_gdps.songs.song_id;
create unique index if not exists gdps_songs_sha256_idx on geobr_gdps.songs (sha256) where sha256 is not null and status <> 'removed';
create index if not exists gdps_songs_name_idx on geobr_gdps.songs (lower(name));
create index if not exists gdps_songs_status_idx on geobr_gdps.songs (status, song_id desc);
create index if not exists gdps_songs_uploader_idx on geobr_gdps.songs (uploader);
create index if not exists gdps_songs_reviewed_by_idx on geobr_gdps.songs (reviewed_by);

-- ------------------------------------------------- roles and permissions
create table if not exists geobr_gdps.roles (
 role_id bigint generated always as identity primary key,
 name text not null unique check (char_length(name) between 2 and 32),
 color int not null default 16777215 check (color between 0 and 16777215),
 permissions text[] not null default '{}',
 position int not null default 100,          -- lower = higher in the hierarchy
 is_owner boolean not null default false,
 created_at timestamptz not null default now()
);
create table if not exists geobr_gdps.account_roles (
 account_id bigint not null references geobr_gdps.accounts(account_id) on delete cascade,
 role_id bigint not null references geobr_gdps.roles(role_id) on delete cascade,
 granted_by bigint references geobr_gdps.accounts(account_id) on delete set null,
 granted_at timestamptz not null default now(),
 primary key (account_id, role_id)
);
create index if not exists gdps_account_roles_role_idx on geobr_gdps.account_roles (role_id);
create index if not exists gdps_account_roles_granted_by_idx on geobr_gdps.account_roles (granted_by);

create table if not exists geobr_gdps.audit_log (
 id bigint generated always as identity primary key,
 actor bigint references geobr_gdps.accounts(account_id) on delete set null,
 action text not null,
 target_type text not null default '',
 target_id text not null default '',
 details jsonb not null default '{}'::jsonb,
 at timestamptz not null default now()
);
create index if not exists gdps_audit_at_idx on geobr_gdps.audit_log (at desc);
create index if not exists gdps_audit_actor_idx on geobr_gdps.audit_log (actor);

-- Initial roles (the Owner can rename, recolour and change permissions).
insert into geobr_gdps.roles (name, color, permissions, position, is_owner) values
 ('Owner', 16733525, array['*'], 0, true),
 ('Admin', 16744192, array['admin.panel','accounts.view','accounts.ban','accounts.manage','levels.rate','levels.feature','levels.suggest','levels.delete','levels.block','comments.moderate','events.manage','leaderboard.moderate','demonlist.manage','songs.moderate','stats.view','audit.view','mod.elder'], 10, false),
 ('Moderator', 5635925, array['admin.panel','accounts.view','accounts.ban','comments.moderate','levels.suggest','stats.view','mod.badge'], 20, false),
 ('Level Moderator', 5592575, array['admin.panel','accounts.view','levels.delete','levels.block','levels.suggest','mod.badge'], 30, false),
 ('Rate Moderator', 16766720, array['admin.panel','levels.rate','levels.feature','levels.suggest','mod.badge'], 30, false),
 ('Event Manager', 11141290, array['admin.panel','events.manage','demonlist.manage'], 40, false),
 ('Leaderboard Moderator', 43775, array['admin.panel','accounts.view','leaderboard.moderate','stats.view'], 40, false)
on conflict (name) do nothing;

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
revoke all on sequence geobr_gdps.song_id_seq from public;

-- ------------------------------------------------------- storage buckets
insert into storage.buckets (id, name, public, file_size_limit, allowed_mime_types) values
 ('geobr-gdps-songs', 'geobr-gdps-songs', true, 15728640, array['audio/mpeg','audio/mp3']),
 ('geobr-gdps-saves', 'geobr-gdps-saves', false, 52428800, null)
on conflict (id) do nothing;

-- ------------------------------------------------------------ release
insert into geobr_private.releases(version, enabled, message)
values ('1.10.0-beta.1', true, 'GeoBR GDPS')
on conflict (version) do nothing;
