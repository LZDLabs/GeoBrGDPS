-- GeoBR 1.6: additive migration. Existing Main Levels and Community data stay intact.
create schema if not exists geobr_private;
revoke all on schema geobr_private from public, anon;
grant usage on schema geobr_private to authenticated;
create table geobr_private.admin_secret (
 singleton boolean primary key default true check(singleton),
 password_hash text not null
);
create table geobr_private.admin_sessions (
 user_id uuid primary key references auth.users(id) on delete cascade,
 expires_at timestamptz not null
);
create table geobr_private.admin_attempts (
 user_id uuid primary key references auth.users(id) on delete cascade,
 window_start timestamptz not null default now(), failures integer not null default 0
);
alter table geobr_private.admin_secret enable row level security;
alter table geobr_private.admin_sessions enable row level security;
alter table geobr_private.admin_attempts enable row level security;
revoke all on all tables in schema geobr_private from public, anon, authenticated;

create function geobr_private.is_admin() returns boolean
language sql stable security definer set search_path=''
as $$ select auth.uid() is not null and exists (
 select 1 from geobr_private.admin_sessions where user_id=auth.uid() and expires_at>now()
) $$;
revoke all on function geobr_private.is_admin() from public,anon;
grant execute on function geobr_private.is_admin() to authenticated;

create function geobr_private.admin_login(p_password text) returns jsonb
language plpgsql security definer set search_path='' as $$
declare a geobr_private.admin_attempts; h text;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 insert into geobr_private.admin_attempts(user_id) values(auth.uid()) on conflict do nothing;
 select * into a from geobr_private.admin_attempts where user_id=auth.uid() for update;
 if a.window_start < now()-interval '15 minutes' then
  update geobr_private.admin_attempts set failures=0,window_start=now() where user_id=auth.uid();
  a.failures=0;
 end if;
 if a.failures>=5 then return jsonb_build_object('ok',false,'error','Aguarde 15 minutos antes de tentar novamente.'); end if;
 select password_hash into h from geobr_private.admin_secret where singleton;
 if h is null or length(p_password)>128 or extensions.crypt(p_password,h) is distinct from h then
  update geobr_private.admin_attempts set failures=failures+1 where user_id=auth.uid();
  return jsonb_build_object('ok',false,'error','Senha incorreta.');
 end if;
 insert into geobr_private.admin_sessions(user_id,expires_at) values(auth.uid(),now()+interval '20 minutes')
 on conflict(user_id) do update set expires_at=excluded.expires_at;
 update geobr_private.admin_attempts set failures=0 where user_id=auth.uid();
 return jsonb_build_object('ok',true,'expires_at',now()+interval '20 minutes');
end $$;
create function public.geobr_admin_login(p_password text) returns jsonb
language sql security invoker set search_path='' as $$ select geobr_private.admin_login(p_password) $$;
create function geobr_private.admin_logout() returns void
language sql security definer set search_path='' as $$ delete from geobr_private.admin_sessions where user_id=auth.uid() $$;
create function public.geobr_admin_logout() returns void
language sql security invoker set search_path='' as $$ select geobr_private.admin_logout() $$;

create table public.geobr_news (
 id bigint generated always as identity primary key,
 title text not null check(length(title) between 1 and 80),
 description text not null check(length(description) between 1 and 1000),
 author text not null default 'Equipe GeoBR' check(length(author)<=40),
 type text not null default 'notice' check(type in ('notice','update','event','maintenance','news')),
 pinned boolean not null default false,
 priority smallint not null default 0 check(priority between 0 and 100),
 published boolean not null default false,
 created_at timestamptz not null default now(), updated_at timestamptz not null default now()
);
create table public.geobr_announcements (
 id bigint generated always as identity primary key,
 title text not null check(length(title) between 1 and 60),
 message text not null check(length(message) between 1 and 200),
 expires_at timestamptz not null default(now()+interval '1 day'),
 published boolean not null default true,
 created_at timestamptz not null default now()
);
create index geobr_announcements_live on public.geobr_announcements(id desc) where published;
create table public.geobr_pass_seasons (
 id bigint generated always as identity primary key,
 name text not null check(length(name) between 1 and 60),
 starts_at timestamptz not null,
 ends_at timestamptz not null,
 xp_per_tier integer not null default 100 check(xp_per_tier between 10 and 10000),
 published boolean not null default false,
 check(ends_at>starts_at),
 exclude using gist(tstzrange(starts_at,ends_at,'[)') with &&) where(published)
);
create table public.geobr_pass_rewards (
 id bigint generated always as identity primary key,
 season_id bigint not null references public.geobr_pass_seasons on delete restrict,
 tier integer not null check(tier between 1 and 100),
 kind text not null check(kind in ('tokens','orbs','stars','skin')),
 amount integer not null default 1 check(amount between 1 and 100000),
 icon_type text not null default 'cube' check(icon_type in ('cube','ship','ball','ufo','wave','robot','spider','swing','jetpack')),
 icon_id integer not null default 1 check(icon_id between 1 and 1000),
 unique(season_id,tier)
);
create table public.geobr_pass_quests (
 id bigint generated always as identity primary key,
 season_id bigint not null references public.geobr_pass_seasons on delete restrict,
 title text not null check(length(title) between 1 and 80),
 event text not null check(event in ('main_normal','main_practice')),
 goal integer not null default 1 check(goal between 1 and 100),
 xp integer not null default 100 check(xp between 1 and 10000),
 daily boolean not null default true
);
create index geobr_pass_quests_season on public.geobr_pass_quests(season_id);
create table public.geobr_wallets (
 user_id uuid primary key references auth.users on delete cascade,
 tokens bigint not null default 0 check(tokens>=0)
);
create table public.geobr_pass_progress (
 user_id uuid not null references auth.users on delete cascade,
 season_id bigint not null references public.geobr_pass_seasons on delete restrict,
 xp bigint not null default 0 check(xp>=0), primary key(user_id,season_id)
);
create index geobr_pass_progress_season on public.geobr_pass_progress(season_id);
create table geobr_private.pass_events (
 user_id uuid not null references auth.users on delete cascade,
 season_id bigint not null references public.geobr_pass_seasons on delete restrict,
 event text not null, level_id bigint not null,
 event_day date not null default (now() at time zone 'utc')::date,
 primary key(user_id,season_id,event,level_id,event_day)
);
create index geobr_events_season on geobr_private.pass_events(season_id);
create table geobr_private.quest_claims (
 user_id uuid not null references auth.users on delete cascade,
 quest_id bigint not null references public.geobr_pass_quests on delete restrict,
 claim_day date not null, xp integer not null, primary key(user_id,quest_id,claim_day)
);
create index geobr_quest_claim_id on geobr_private.quest_claims(quest_id);
create table public.geobr_reward_claims (
 id uuid primary key default gen_random_uuid(),
 user_id uuid not null references auth.users on delete cascade,
 reward_id bigint not null references public.geobr_pass_rewards on delete restrict,
 reward jsonb not null, created_at timestamptz not null default now(),
 unique(user_id,reward_id)
);
create index geobr_reward_claim_reward on public.geobr_reward_claims(reward_id);
alter table geobr_private.pass_events enable row level security;
alter table geobr_private.quest_claims enable row level security;

-- Published catalogs are public. Only server-validated administrators can mutate.
do $$
declare t text;
begin
 foreach t in array array['geobr_news','geobr_announcements','geobr_pass_seasons'] loop
  execute format('alter table public.%I enable row level security',t);
  execute format('grant select on public.%I to anon,authenticated',t);
  execute format('grant insert,update,delete on public.%I to authenticated',t);
  execute format('create policy published_read on public.%I for select to anon,authenticated using(published)',t);
  execute format('create policy admin_all on public.%I for all to authenticated using((select geobr_private.is_admin())) with check((select geobr_private.is_admin()))',t);
 end loop;
 foreach t in array array['geobr_pass_rewards','geobr_pass_quests'] loop
  execute format('alter table public.%I enable row level security',t);
  execute format('grant select on public.%I to anon,authenticated',t);
  execute format('grant insert,update,delete on public.%I to authenticated',t);
  execute format('create policy published_read on public.%I for select to anon,authenticated using(exists(select 1 from public.geobr_pass_seasons s where s.id=season_id and s.published))',t);
  execute format('create policy admin_all on public.%I for all to authenticated using((select geobr_private.is_admin())) with check((select geobr_private.is_admin()))',t);
 end loop;
 foreach t in array array['geobr_wallets','geobr_pass_progress','geobr_reward_claims'] loop
  execute format('alter table public.%I enable row level security',t);
  execute format('grant select on public.%I to authenticated',t);
  execute format('revoke insert,update,delete on public.%I from anon,authenticated',t);
  execute format('create policy own_read on public.%I for select to authenticated using((select auth.uid())=user_id)',t);
 end loop;
end $$;
grant usage,select on sequence public.geobr_news_id_seq,public.geobr_announcements_id_seq,
 public.geobr_pass_seasons_id_seq,public.geobr_pass_rewards_id_seq,public.geobr_pass_quests_id_seq to authenticated;

alter table public.geobr_main_levels enable row level security;
grant insert,update,delete,select on public.geobr_main_levels to authenticated;
grant usage,select on sequence public.geobr_main_levels_id_seq to authenticated;
create policy geobr_admin_main_levels on public.geobr_main_levels for all to authenticated
 using((select geobr_private.is_admin())) with check((select geobr_private.is_admin()));
create policy geobr_admin_storage on storage.objects for all to authenticated
 using(bucket_id='geobr-main-levels' and (select geobr_private.is_admin()))
 with check(bucket_id='geobr-main-levels' and (select geobr_private.is_admin()));
create function geobr_private.touch_updated_at() returns trigger language plpgsql security invoker set search_path='' as $$ begin new.updated_at=now(); return new; end $$;
create trigger geobr_news_updated before update on public.geobr_news
 for each row execute function geobr_private.touch_updated_at();

-- Published reward/quest definitions are immutable after a player has claimed them.
create function geobr_private.lock_claimed_definition() returns trigger
language plpgsql security definer set search_path='' as $$
begin
 if TG_TABLE_NAME='geobr_pass_rewards' and exists(select 1 from public.geobr_reward_claims where reward_id=old.id) then
  raise exception 'Esta recompensa ja foi resgatada. Configure outra temporada.';
 elsif TG_TABLE_NAME='geobr_pass_quests' and exists(select 1 from geobr_private.quest_claims where quest_id=old.id) then
  raise exception 'Esta Quest Pass ja tem resgates. Configure outra quest.';
 end if;
 return case when TG_OP='DELETE' then old else new end;
end $$;
create trigger reward_definition_lock before update or delete on public.geobr_pass_rewards
 for each row execute function geobr_private.lock_claimed_definition();
create trigger quest_definition_lock before update or delete on public.geobr_pass_quests
 for each row execute function geobr_private.lock_claimed_definition();

create function geobr_private.pass_state() returns jsonb
language plpgsql security definer set search_path='' as $$
declare s public.geobr_pass_seasons; v_xp bigint; v_tokens bigint; d date=(now() at time zone 'utc')::date;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 select * into s from public.geobr_pass_seasons where published and starts_at<=now() and ends_at>now() limit 1;
 select tokens into v_tokens from public.geobr_wallets where user_id=auth.uid();
 if s.id is null then return jsonb_build_object('tokens',coalesce(v_tokens,0),'season',null,'quests','[]'::jsonb,'rewards','[]'::jsonb); end if;
 select xp into v_xp from public.geobr_pass_progress where user_id=auth.uid() and season_id=s.id;
 return jsonb_build_object('season',to_jsonb(s),'xp',coalesce(v_xp,0),'tokens',coalesce(v_tokens,0),
  'rewards',coalesce((select jsonb_agg(to_jsonb(r)||jsonb_build_object('claimed',exists(select 1 from public.geobr_reward_claims c where c.reward_id=r.id and c.user_id=auth.uid())) order by tier) from public.geobr_pass_rewards r where season_id=s.id),'[]'::jsonb),
  'quests',coalesce((select jsonb_agg(to_jsonb(q)||jsonb_build_object(
   'progress',least(q.goal,(select count(distinct level_id) from geobr_private.pass_events e where e.user_id=auth.uid() and e.season_id=s.id and e.event=q.event and (not q.daily or e.event_day=d))),
   'claimed',exists(select 1 from geobr_private.quest_claims c where c.quest_id=q.id and c.user_id=auth.uid() and c.claim_day=case when q.daily then d else date '1970-01-01' end)) order by q.id)
  from public.geobr_pass_quests q where season_id=s.id),'[]'::jsonb));
end $$;
create function public.geobr_pass_state() returns jsonb language sql security invoker set search_path=''
 as $$ select geobr_private.pass_state() $$;

create function geobr_private.record_completion(p_level_id bigint,p_practice boolean) returns jsonb
language plpgsql security definer set search_path='' as $$
declare sid bigint;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 -- The catalog is authoritative; a client cannot submit arbitrary XP or quantities.
 if not exists(select 1 from public.geobr_main_levels where id=p_level_id and published) then raise exception 'Main Level indisponivel'; end if;
 select id into sid from public.geobr_pass_seasons where published and starts_at<=now() and ends_at>now() limit 1;
 if sid is not null then
  insert into geobr_private.pass_events(user_id,season_id,event,level_id)
  values(auth.uid(),sid,case when p_practice then 'main_practice' else 'main_normal' end,p_level_id) on conflict do nothing;
 end if;
 return geobr_private.pass_state();
end $$;
create function public.geobr_pass_complete(p_level_id bigint,p_practice boolean) returns jsonb
language sql security invoker set search_path='' as $$ select geobr_private.record_completion(p_level_id,p_practice) $$;

create function geobr_private.claim_quest(p_quest_id bigint) returns jsonb
language plpgsql security definer set search_path='' as $$
declare q public.geobr_pass_quests; d date=(now() at time zone 'utc')::date; c integer; inserted integer;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 select * into q from public.geobr_pass_quests where id=p_quest_id for share;
 if q.id is null or not exists(select 1 from public.geobr_pass_seasons where id=q.season_id and published and starts_at<=now() and ends_at>now()) then raise exception 'Quest Pass indisponivel'; end if;
 select count(distinct level_id) into c from geobr_private.pass_events where user_id=auth.uid() and season_id=q.season_id and event=q.event and (not q.daily or event_day=d);
 if c<q.goal then raise exception 'Complete a Quest Pass primeiro'; end if;
 insert into geobr_private.quest_claims(user_id,quest_id,claim_day,xp)
 values(auth.uid(),q.id,case when q.daily then d else date '1970-01-01' end,q.xp) on conflict do nothing;
 get diagnostics inserted=row_count;
 if inserted>0 then
  insert into public.geobr_pass_progress(user_id,season_id,xp) values(auth.uid(),q.season_id,q.xp)
  on conflict(user_id,season_id) do update set xp=public.geobr_pass_progress.xp+excluded.xp;
 end if;
 return geobr_private.pass_state();
end $$;
create function public.geobr_pass_claim_quest(p_quest_id bigint) returns jsonb
language sql security invoker set search_path='' as $$ select geobr_private.claim_quest(p_quest_id) $$;

create function geobr_private.claim_reward(p_reward_id bigint) returns jsonb
language plpgsql security definer set search_path='' as $$
declare r public.geobr_pass_rewards; threshold integer; inserted integer;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 select * into r from public.geobr_pass_rewards where id=p_reward_id for share;
 select xp_per_tier*r.tier into threshold from public.geobr_pass_seasons where id=r.season_id and published and starts_at<=now() and ends_at>now();
 if threshold is null or coalesce((select xp from public.geobr_pass_progress where user_id=auth.uid() and season_id=r.season_id),0)<threshold then raise exception 'XP insuficiente ou temporada encerrada'; end if;
 insert into public.geobr_reward_claims(user_id,reward_id,reward) values(auth.uid(),r.id,to_jsonb(r)) on conflict(user_id,reward_id) do nothing;
 get diagnostics inserted=row_count;
 if inserted>0 and r.kind='tokens' then
  insert into public.geobr_wallets(user_id,tokens) values(auth.uid(),r.amount)
  on conflict(user_id) do update set tokens=public.geobr_wallets.tokens+excluded.tokens;
 end if;
 return geobr_private.pass_state();
end $$;
create function public.geobr_pass_claim_reward(p_reward_id bigint) returns jsonb
language sql security invoker set search_path='' as $$ select geobr_private.claim_reward(p_reward_id) $$;
-- Seal every privileged implementation and explicitly expose only intended RPCs.
revoke all on all functions in schema geobr_private from public,anon;
grant execute on function geobr_private.admin_login(text),geobr_private.admin_logout(),
 geobr_private.pass_state(),geobr_private.record_completion(bigint,boolean),
 geobr_private.claim_quest(bigint),geobr_private.claim_reward(bigint) to authenticated;
revoke all on function public.geobr_admin_login(text),public.geobr_admin_logout(),
 public.geobr_pass_state(),public.geobr_pass_complete(bigint,boolean),
 public.geobr_pass_claim_quest(bigint),public.geobr_pass_claim_reward(bigint) from public,anon;
grant execute on function public.geobr_admin_login(text),public.geobr_admin_logout(),
 public.geobr_pass_state(),public.geobr_pass_complete(bigint,boolean),
 public.geobr_pass_claim_quest(bigint),public.geobr_pass_claim_reward(bigint) to authenticated;

-- A usable first season; subsequent changes are made in the in-game ADM panel.
with s as (
 insert into public.geobr_pass_seasons(name,starts_at,ends_at,xp_per_tier,published)
 values('Temporada 1 - Origens',now(),now()+interval '30 days',100,true) returning id
)
insert into public.geobr_pass_rewards(season_id,tier,kind,amount,icon_type,icon_id)
select s.id,v.tier,v.kind,v.amount,'cube',v.icon_id from s cross join (values
 (1,'tokens',25,1),(2,'orbs',100,1),(3,'skin',1,3),(4,'stars',1,1),(5,'tokens',50,1),
 (6,'orbs',150,1),(7,'skin',1,4),(8,'stars',2,1),(9,'tokens',75,1),(10,'orbs',300,1)
) as v(tier,kind,amount,icon_id);
insert into public.geobr_pass_quests(season_id,title,event,goal,xp,daily)
select id,'Conclua 1 Main Level no Normal','main_normal',1,100,true from public.geobr_pass_seasons where name='Temporada 1 - Origens';
insert into public.geobr_pass_quests(season_id,title,event,goal,xp,daily)
select id,'Conclua 1 Main Level na Pratica','main_practice',1,100,true from public.geobr_pass_seasons where name='Temporada 1 - Origens';
;

