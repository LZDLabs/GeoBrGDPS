-- Local test stand-ins for the Supabase objects the GDPS migration touches (NOT for production).
do $$ begin
 if not exists (select 1 from pg_roles where rolname='anon') then create role anon nologin; end if;
 if not exists (select 1 from pg_roles where rolname='authenticated') then create role authenticated nologin; end if;
 if not exists (select 1 from pg_roles where rolname='service_role') then create role service_role nologin bypassrls; end if;
end $$;
create schema if not exists extensions;
create extension if not exists pgcrypto with schema extensions;
create schema if not exists auth;
create table if not exists auth.users(id uuid primary key default gen_random_uuid(), is_anonymous boolean default true);
create schema if not exists storage;
create table if not exists storage.buckets(id text primary key, name text not null, public boolean default false, file_size_limit bigint, allowed_mime_types text[]);
create schema if not exists geobr_private;
create table if not exists geobr_private.releases(version text primary key, enabled boolean not null default true, message text not null default 'x', updated_at timestamptz not null default now());
create table if not exists geobr_private.admin_secret(singleton boolean primary key default true, password_hash text not null);
insert into geobr_private.admin_secret values (true, extensions.crypt('adm-local-test', extensions.gen_salt('bf'))) on conflict do nothing;
