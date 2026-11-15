-- GeoBR 1.7. Additive: keep earned XP, Tokens, claims and all published content.
create table public.geobr_quest_steps (
 id bigint generated always as identity primary key,
 season_id bigint not null references public.geobr_pass_seasons(id) on delete restrict,
 sequence_order integer not null check(sequence_order between 1 and 1000),
 title text not null check(char_length(title) between 1 and 80),
 kind text not null check(kind in ('complete','death','reach')),
 goal integer not null default 1 check(goal between 1 and 100),
 difficulty text not null default 'any' check(difficulty in ('any','auto','easy','normal','hard','harder','insane','demon')),
 scope text not null default 'any' check(scope in ('any','main','online','official')),
 mode text not null default 'normal' check(mode in ('any','normal','practice')),
 platformer text not null default 'any' check(platformer in ('any','classic','platformer')),
 min_percent integer not null default 0 check(min_percent between 0 and 100),
 max_percent integer not null default 100 check(max_percent between 0 and 100),
 xp integer not null default 100 check(xp between 1 and 10000),
 tokens integer not null default 8 check(tokens between 0 and 1000),
 published boolean not null default true,
 unique(season_id,sequence_order),check(max_percent>=min_percent)
);
create table public.geobr_profiles (
 user_id uuid primary key references auth.users(id) on delete cascade,
 display_name text not null default 'Jogador GeoBR' check(char_length(display_name) between 1 and 40),
 lifetime_xp bigint not null default 0 check(lifetime_xp between 0 and 1000000000000),
 theme_slug text not null default 'default',title_name text not null default 'Iniciante',
 updated_at timestamptz not null default now()
);
create table public.geobr_shop_items (
 id text primary key check(id ~ '^[a-z0-9_-]{1,40}$'),
 name text not null check(char_length(name) between 1 and 32),
 kind text not null check(kind in ('theme','title','tier_skip','quest_skip')),
 price integer not null check(price between 0 and 100000),
 style text not null default '' check(char_length(style)<=32),
 description text not null default '' check(char_length(description)<=180),
 sort_order integer not null default 0,published boolean not null default true,
 check(kind<>'theme' or style in ('fire','ice','forest','cosmos','neon','gold'))
);
create table public.geobr_inventory (
 user_id uuid references auth.users(id) on delete cascade,
 item_id text references public.geobr_shop_items(id) on delete restrict,
 acquired_at timestamptz not null default now(),primary key(user_id,item_id)
);
create table geobr_private.quest_runs (
 user_id uuid references auth.users(id) on delete cascade,
 season_id bigint references public.geobr_pass_seasons(id) on delete restrict,
 step_id bigint not null references public.geobr_quest_steps(id) on delete restrict,
 run_id uuid not null default gen_random_uuid(),definition jsonb not null,
 progress integer not null default 0,completed_total integer not null default 0,
 started_at timestamptz not null default now(),primary key(user_id,season_id)
);
create table geobr_private.activity_receipts (
 user_id uuid references auth.users(id) on delete cascade,
 event_id uuid not null,run_id uuid not null,created_at timestamptz not null default now(),
 primary key(user_id,event_id)
);
create table geobr_private.quest_distinct (
 user_id uuid references auth.users(id) on delete cascade,
 run_id uuid not null,match_key text not null,primary key(user_id,run_id,match_key)
);
create table geobr_private.quest_history (
 user_id uuid references auth.users(id) on delete cascade,
 run_id uuid not null,season_id bigint not null,step_id bigint not null,
 skipped boolean not null,xp integer not null,tokens integer not null,
 created_at timestamptz not null default now(),primary key(user_id,run_id)
);
create table geobr_private.shop_orders (
 user_id uuid references auth.users(id) on delete cascade,
 request_id uuid not null,item_id text not null,paid integer not null,
 created_at timestamptz not null default now(),primary key(user_id,request_id)
);
-- Backfill permanent experience, preserving the former pass progress.
insert into public.geobr_profiles(user_id,lifetime_xp)
 select user_id,sum(xp) from public.geobr_pass_progress group by user_id on conflict do nothing;

alter table public.geobr_quest_steps enable row level security;
alter table public.geobr_profiles enable row level security;
alter table public.geobr_shop_items enable row level security;
alter table public.geobr_inventory enable row level security;
alter table geobr_private.quest_runs enable row level security;
alter table geobr_private.activity_receipts enable row level security;
alter table geobr_private.quest_distinct enable row level security;
alter table geobr_private.quest_history enable row level security;
alter table geobr_private.shop_orders enable row level security;
revoke all on public.geobr_quest_steps,public.geobr_profiles,public.geobr_shop_items,public.geobr_inventory from anon,authenticated;
revoke all on geobr_private.quest_runs,geobr_private.activity_receipts,geobr_private.quest_distinct,geobr_private.quest_history,geobr_private.shop_orders from public,anon,authenticated;
grant select on public.geobr_quest_steps,public.geobr_profiles,public.geobr_shop_items to anon,authenticated;
grant select on public.geobr_inventory to authenticated;
grant insert,update,delete on public.geobr_quest_steps,public.geobr_shop_items to authenticated;
grant usage,select on sequence public.geobr_quest_steps_id_seq to authenticated;
create policy "Quest catalog published" on public.geobr_quest_steps for select to anon,authenticated
 using(published and exists(select 1 from public.geobr_pass_seasons s where s.id=season_id and s.published));
create policy "Quest catalog admin" on public.geobr_quest_steps for all to authenticated
 using((select geobr_private.is_admin())) with check((select geobr_private.is_admin()));
create policy "Shop published" on public.geobr_shop_items for select to anon,authenticated using(published);
create policy "Shop admin" on public.geobr_shop_items for all to authenticated
 using((select geobr_private.is_admin())) with check((select geobr_private.is_admin()));
create policy "Cosmetic profiles public" on public.geobr_profiles for select to anon,authenticated using(true);
create policy "Inventory belongs to user" on public.geobr_inventory for select to authenticated using(user_id=(select auth.uid()));
create index geobr_inventory_item_idx on public.geobr_inventory(item_id);
create index geobr_quest_runs_step_idx on geobr_private.quest_runs(step_id);

create function geobr_private.ensure_v2() returns void language plpgsql security definer set search_path='' as $$
declare sid bigint; q public.geobr_quest_steps;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 insert into public.geobr_profiles(user_id,lifetime_xp) values(auth.uid(),coalesce((select sum(xp) from public.geobr_pass_progress where user_id=auth.uid()),0)) on conflict do nothing;
 insert into public.geobr_wallets(user_id,tokens) values(auth.uid(),0) on conflict do nothing;
 select id into sid from public.geobr_pass_seasons where published and starts_at<=now() and ends_at>now() limit 1;
 if sid is not null then
  select * into q from public.geobr_quest_steps where season_id=sid and published order by sequence_order limit 1;
  if q.id is not null then
   insert into geobr_private.quest_runs(user_id,season_id,step_id,definition) values(auth.uid(),sid,q.id,to_jsonb(q)) on conflict do nothing;
  end if;
 end if;
end $$;
create function geobr_private.state_v2() returns jsonb language plpgsql security definer set search_path='' as $$
declare base jsonb; profile public.geobr_profiles; sid bigint; lv bigint; quest jsonb;
begin
 perform geobr_private.ensure_v2();
 base=geobr_private.pass_state()-'quests'; sid=(base->'season'->>'id')::bigint;
 select * into profile from public.geobr_profiles where user_id=auth.uid();
 lv=floor(sqrt(profile.lifetime_xp::numeric/100))::bigint+1;
 select definition||jsonb_build_object('run_id',run_id,'progress',progress,'completed_total',completed_total) into quest
 from geobr_private.quest_runs where user_id=auth.uid() and season_id=sid;
 return base||jsonb_build_object(
  'profile',to_jsonb(profile)||jsonb_build_object('level',lv,'level_start_xp',100*(lv-1)*(lv-1),'next_level_xp',100*lv*lv),
  'quest',quest,'quest_count',(select count(*) from public.geobr_quest_steps where season_id=sid and published),
  'shop',coalesce((select jsonb_agg(to_jsonb(i)||jsonb_build_object(
   'owned',exists(select 1 from public.geobr_inventory v where v.user_id=auth.uid() and v.item_id=i.id),
   'equipped',case when kind='theme' then i.style=profile.theme_slug when kind='title' then i.name=profile.title_name else false end)
   order by sort_order,id) from public.geobr_shop_items i where published),'[]'::jsonb));
end $$;
create function geobr_private.advance_quest(p_season bigint,p_skip boolean) returns void language plpgsql security definer set search_path='' as $$
declare r geobr_private.quest_runs; q public.geobr_quest_steps; points integer; coins integer;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 select * into r from geobr_private.quest_runs where user_id=auth.uid() and season_id=p_season for update;
 if not found then raise exception 'Quest indisponivel'; end if;
 points=case when p_skip then 0 else (r.definition->>'xp')::int end;
 coins=case when p_skip then 0 else (r.definition->>'tokens')::int end;
 insert into geobr_private.quest_history(user_id,run_id,season_id,step_id,skipped,xp,tokens)
 values(auth.uid(),r.run_id,p_season,r.step_id,p_skip,points,coins);
 if not p_skip then
  insert into public.geobr_pass_progress(user_id,season_id,xp) values(auth.uid(),p_season,points)
  on conflict(user_id,season_id) do update set xp=public.geobr_pass_progress.xp+excluded.xp;
  update public.geobr_profiles set lifetime_xp=lifetime_xp+points,updated_at=now() where user_id=auth.uid();
  update public.geobr_wallets set tokens=tokens+coins where user_id=auth.uid();
 end if;
 select * into q from public.geobr_quest_steps where season_id=p_season and published and sequence_order>(r.definition->>'sequence_order')::int order by sequence_order limit 1;
 if not found then select * into q from public.geobr_quest_steps where season_id=p_season and published order by sequence_order limit 1; end if;
 if q.id is null then raise exception 'Nenhuma quest publicada'; end if;
 update geobr_private.quest_runs set step_id=q.id,definition=to_jsonb(q),run_id=gen_random_uuid(),progress=0,
  completed_total=completed_total+1,started_at=now() where user_id=auth.uid() and season_id=p_season;
end $$;
create function geobr_private.record_activity(p_event jsonb) returns jsonb language plpgsql security definer set search_path='' as $$
declare sid bigint; r geobr_private.quest_runs; q jsonb; event_id uuid; attempt uuid; run uuid; kind text;
 scope text; levelkey text; diff text; mode text; plat text; pct integer; inserted integer; matches boolean; level public.geobr_main_levels;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));
 perform geobr_private.ensure_v2();
 event_id=(p_event->>'id')::uuid;attempt=(p_event->>'attempt')::uuid;run=(p_event->>'run')::uuid;
 if event_id is null or attempt is null or run is null then raise exception 'Evento incompleto'; end if;
 select id into sid from public.geobr_pass_seasons where published and starts_at<=now() and ends_at>now() limit 1;
 select * into r from geobr_private.quest_runs where user_id=auth.uid() and season_id=sid for update;
 if r.run_id is null or r.run_id<>run then return geobr_private.state_v2(); end if;
 insert into geobr_private.activity_receipts(user_id,event_id,run_id) values(auth.uid(),event_id,run) on conflict do nothing;
 get diagnostics inserted=row_count;if inserted=0 then return geobr_private.state_v2(); end if;
 kind=p_event->>'kind';scope=p_event->>'scope';levelkey=p_event->>'level';diff=p_event->>'difficulty';mode=p_event->>'mode';plat=p_event->>'platformer';pct=(p_event->>'percent')::int;
 if kind is null or kind not in ('complete','death','reach') or scope is null or scope not in ('main','online','official') or
  mode is null or mode not in ('normal','practice') or plat is null or plat not in ('classic','platformer') or
  pct is null or pct not between 0 and 100 or levelkey is null or levelkey !~ '^(main|online|official):[1-9][0-9]{0,9}$' or split_part(levelkey,':',1)<>scope then raise exception 'Evento invalido'; end if;
 if scope='main' then
  select * into level from public.geobr_main_levels where id=split_part(levelkey,':',2)::bigint and published;
  if not found then return geobr_private.state_v2(); end if;
  diff=case when lower(level.difficulty) like '%demon%' then 'demon' else lower(level.difficulty) end;
  plat=case when level.platformer then 'platformer' else 'classic' end;
 end if;
 if diff is null or diff not in ('auto','easy','normal','hard','harder','insane','demon','unrated') then raise exception 'Dificuldade invalida'; end if;
 q=r.definition;
 matches=((q->>'kind'=kind) or (q->>'kind'='reach' and kind in ('death','complete','reach')))
  and (q->>'scope'='any' or q->>'scope'=scope) and (q->>'difficulty'='any' or q->>'difficulty'=diff)
  and (q->>'mode'='any' or q->>'mode'=mode) and (q->>'platformer'='any' or q->>'platformer'=plat)
  and pct between (q->>'min_percent')::int and (q->>'max_percent')::int
  and (kind<>'complete' or pct=100);
 if matches then
  insert into geobr_private.quest_distinct(user_id,run_id,match_key) values(auth.uid(),run,
    case when q->>'kind' in ('complete','reach') then levelkey else attempt::text end) on conflict do nothing;
  get diagnostics inserted=row_count;
  if inserted>0 then
   update geobr_private.quest_runs set progress=progress+1 where user_id=auth.uid() and season_id=sid;
   if r.progress+1 >= (q->>'goal')::int then perform geobr_private.advance_quest(sid,false); end if;
  end if;
 end if;
 return geobr_private.state_v2();
end $$;
create function geobr_private.buy_item(p_item_id text,p_request_id uuid,p_expected_price integer,p_expected_run uuid default null,p_expected_tier integer default null)
returns jsonb language plpgsql security definer set search_path='' as $$
declare item public.geobr_shop_items; old geobr_private.shop_orders; sid bigint; per integer; xp bigint; run uuid; count integer;
begin
 if auth.uid() is null or p_request_id is null then raise exception 'Sessao e pedido necessarios'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));perform geobr_private.ensure_v2();
 select * into old from geobr_private.shop_orders where user_id=auth.uid() and request_id=p_request_id;
 if found then
  if old.item_id<>p_item_id then raise exception 'Pedido pertence a outro item'; end if;
  return geobr_private.state_v2();
 end if;
 select * into item from public.geobr_shop_items where id=p_item_id and published for share;
 if item.id is null then raise exception 'Item indisponivel'; end if;
 if p_expected_price is null or item.price<>p_expected_price then raise exception 'Preco mudou; atualize a loja'; end if;
 if item.kind in ('theme','title') and exists(select 1 from public.geobr_inventory where user_id=auth.uid() and item_id=item.id) then
  insert into geobr_private.shop_orders(user_id,request_id,item_id,paid) values(auth.uid(),p_request_id,item.id,0);
  return geobr_private.state_v2();
 end if;
 if item.kind in ('tier_skip','quest_skip') then
  select id,xp_per_tier into sid,per from public.geobr_pass_seasons where published and starts_at<=now() and ends_at>now() limit 1;
  if sid is null then raise exception 'Nenhuma temporada ativa'; end if;
  if item.kind='quest_skip' then
   select run_id into run from geobr_private.quest_runs where user_id=auth.uid() and season_id=sid for update;
   if run is null or p_expected_run is null or run<>p_expected_run then raise exception 'Quest mudou; confira a quest atual'; end if;
  else
   select coalesce((select p.xp from public.geobr_pass_progress p where p.user_id=auth.uid() and season_id=sid),0) into xp;
   if p_expected_tier is null or xp/per<>p_expected_tier then raise exception 'Etapa mudou; confira o passe'; end if;
   if xp/per>=coalesce((select max(tier) from public.geobr_pass_rewards where season_id=sid),0) then raise exception 'Todas as etapas ja foram desbloqueadas'; end if;
  end if;
 end if;
 update public.geobr_wallets set tokens=tokens-item.price where user_id=auth.uid() and tokens>=item.price;
 get diagnostics count=row_count;if count<>1 then raise exception 'Tokens insuficientes'; end if;
 if item.kind in ('theme','title') then
  insert into public.geobr_inventory(user_id,item_id) values(auth.uid(),item.id);
 elsif item.kind='quest_skip' then perform geobr_private.advance_quest(sid,true);
 else
  insert into public.geobr_pass_progress(user_id,season_id,xp) values(auth.uid(),sid,(xp/per+1)*per)
  on conflict(user_id,season_id) do update set xp=excluded.xp;
 end if;
 insert into geobr_private.shop_orders(user_id,request_id,item_id,paid) values(auth.uid(),p_request_id,item.id,item.price);
 return geobr_private.state_v2();
end $$;
create function geobr_private.equip_item(p_item_id text) returns jsonb language plpgsql security definer set search_path='' as $$
declare item public.geobr_shop_items;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));perform geobr_private.ensure_v2();
 select * into item from public.geobr_shop_items where id=p_item_id and kind in ('theme','title');
 if item.id is null or not exists(select 1 from public.geobr_inventory where user_id=auth.uid() and item_id=item.id) then raise exception 'Compre este item primeiro'; end if;
 update public.geobr_profiles set theme_slug=case when item.kind='theme' then item.style else theme_slug end,
  title_name=case when item.kind='title' then item.name else title_name end,updated_at=now() where user_id=auth.uid();
 return geobr_private.state_v2();
end $$;
create function geobr_private.profile_name(p_name text) returns void language plpgsql security definer set search_path='' as $$
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 if p_name is null or char_length(trim(p_name)) not between 1 and 40 then raise exception 'Nome invalido'; end if;
 perform geobr_private.ensure_v2();
 update public.geobr_profiles set display_name=trim(p_name),updated_at=now() where user_id=auth.uid() and display_name<>trim(p_name);
end $$;
-- Old clients may still view/claim already earned tiers, but may not farm the retired parallel quests.
create or replace function geobr_private.claim_quest(p_quest_id bigint) returns jsonb language plpgsql security definer set search_path='' as $$
begin raise exception 'Atualize o GeoBR para usar as quests em sequencia'; end $$;

create function public.geobr_state_v2() returns jsonb language sql security invoker set search_path='' as $$select geobr_private.state_v2()$$;
create function public.geobr_activity(p_event jsonb) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.record_activity(p_event)$$;
create function public.geobr_buy_item(p_item_id text,p_request_id uuid,p_expected_price integer,p_expected_run uuid default null,p_expected_tier integer default null)
returns jsonb language sql security invoker set search_path='' as $$select geobr_private.buy_item(p_item_id,p_request_id,p_expected_price,p_expected_run,p_expected_tier)$$;
create function public.geobr_equip_item(p_item_id text) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.equip_item(p_item_id)$$;
create function public.geobr_profile_name(p_name text) returns void language sql security invoker set search_path='' as $$select geobr_private.profile_name(p_name)$$;

revoke all on function geobr_private.ensure_v2(),geobr_private.state_v2(),geobr_private.advance_quest(bigint,boolean),geobr_private.record_activity(jsonb),geobr_private.buy_item(text,uuid,integer,uuid,integer),geobr_private.equip_item(text),geobr_private.profile_name(text) from public,anon,authenticated;
grant execute on function geobr_private.state_v2(),geobr_private.record_activity(jsonb),geobr_private.buy_item(text,uuid,integer,uuid,integer),geobr_private.equip_item(text),geobr_private.profile_name(text) to authenticated;
revoke all on function public.geobr_state_v2(),public.geobr_activity(jsonb),public.geobr_buy_item(text,uuid,integer,uuid,integer),public.geobr_equip_item(text),public.geobr_profile_name(text) from public,anon;
grant execute on function public.geobr_state_v2(),public.geobr_activity(jsonb),public.geobr_buy_item(text,uuid,integer,uuid,integer),public.geobr_equip_item(text),public.geobr_profile_name(text) to authenticated;

insert into public.geobr_quest_steps(season_id,sequence_order,title,kind,goal,difficulty,scope,mode,platformer,min_percent,max_percent,xp,tokens)
select s.id,v.* from public.geobr_pass_seasons s cross join (values
(1,'Complete 2 Autos diferentes','complete',2,'auto','any','normal','any',0,100,125,8),
(2,'Morra exatamente em 46%','death',1,'any','any','normal','classic',46,46,100,8),
(3,'Complete um level no normal','complete',1,'any','any','normal','any',0,100,100,8),
(4,'Chegue a 25% em um level','reach',1,'any','any','normal','classic',25,100,100,8),
(5,'Complete um level na pratica','complete',1,'any','any','practice','any',0,100,100,8),
(6,'Morra exatamente em 25%','death',1,'any','any','normal','classic',25,25,100,8),
(7,'Complete 2 Easy diferentes','complete',2,'easy','any','normal','any',0,100,125,8),
(8,'Complete um level de plataforma','complete',1,'any','any','normal','platformer',0,100,100,8),
(9,'Chegue a 50% em um level','reach',1,'any','any','normal','classic',50,100,100,8),
(10,'Tente novamente: morra em 2 tentativas','death',2,'any','any','normal','any',0,100,100,8),
(11,'Complete um Main Level GeoBR','complete',1,'any','main','normal','any',0,100,100,8),
(12,'Treine um Main Level GeoBR','complete',1,'any','main','practice','any',0,100,100,8),
(13,'Complete 2 levels Normal diferentes','complete',2,'normal','any','normal','any',0,100,125,8),
(14,'Morra exatamente em 75%','death',1,'any','any','normal','classic',75,75,100,8),
(15,'Chegue a 80% em um level','reach',1,'any','any','normal','classic',80,100,100,8),
(16,'Complete 2 levels oficiais do GD','complete',2,'any','official','normal','any',0,100,125,8),
(17,'Complete 2 levels online diferentes','complete',2,'any','online','normal','any',0,100,125,8),
(18,'Quase la: morra entre 90% e 99%','death',1,'any','any','normal','classic',90,99,100,8),
(19,'Complete um Hard','complete',1,'hard','any','normal','any',0,100,125,8),
(20,'Treine um level de plataforma','complete',1,'any','any','practice','platformer',0,100,100,8),
(21,'Treine um Auto','complete',1,'auto','any','practice','any',0,100,100,8),
(22,'Complete 2 Auto diferentes','complete',2,'auto','any','normal','any',0,100,150,8),
(23,'Treine um Easy','complete',1,'easy','any','practice','any',0,100,100,8),
(24,'Complete 2 Easy diferentes','complete',2,'easy','any','normal','any',0,100,150,8),
(25,'Treine um Normal','complete',1,'normal','any','practice','any',0,100,100,8),
(26,'Complete 2 Normal diferentes','complete',2,'normal','any','normal','any',0,100,150,8),
(27,'Treine um Hard','complete',1,'hard','any','practice','any',0,100,100,8),
(28,'Complete um Hard','complete',1,'hard','any','normal','any',0,100,150,8),
(29,'Treine um Harder','complete',1,'harder','any','practice','any',0,100,100,8),
(30,'Complete um Harder','complete',1,'harder','any','normal','any',0,100,150,8),
(31,'Treine um Insane','complete',1,'insane','any','practice','any',0,100,125,8),
(32,'Complete um Insane','complete',1,'insane','any','normal','any',0,100,150,8),
(33,'Treine um Demon','complete',1,'demon','any','practice','any',0,100,125,8),
(34,'Complete um Demon','complete',1,'demon','any','normal','any',0,100,150,8),
(35,'Morra exatamente em 10%','death',1,'any','any','normal','classic',10,10,125,8),
(36,'Morra exatamente em 20%','death',1,'any','any','normal','classic',20,20,125,8),
(37,'Morra exatamente em 33%','death',1,'any','any','normal','classic',33,33,125,8),
(38,'Morra exatamente em 40%','death',1,'any','any','normal','classic',40,40,125,8),
(39,'Morra exatamente em 55%','death',1,'any','any','normal','classic',55,55,125,8),
(40,'Morra exatamente em 66%','death',1,'any','any','normal','classic',66,66,125,8),
(41,'Morra exatamente em 70%','death',1,'any','any','normal','classic',70,70,125,8),
(42,'Morra exatamente em 85%','death',1,'any','any','normal','classic',85,85,125,8),
(43,'Morra exatamente em 95%','death',1,'any','any','normal','classic',95,95,125,8),
(44,'Morra exatamente em 99%','death',1,'any','any','normal','classic',99,99,125,8),
(45,'Chegue a 10% em 2 levels','reach',2,'any','any','normal','classic',10,100,125,8),
(46,'Chegue a 35% em 2 levels','reach',2,'any','any','normal','classic',35,100,125,8),
(47,'Chegue a 60% em 2 levels','reach',2,'any','any','normal','classic',60,100,125,8),
(48,'Chegue a 75% em 2 levels','reach',2,'any','any','normal','classic',75,100,125,8),
(49,'Chegue a 90% em 2 levels','reach',2,'any','any','normal','classic',90,100,125,8),
(50,'Chegue a 95% em 2 levels','reach',2,'any','any','normal','classic',95,100,125,8),
(51,'Morra entre 1% e 9% duas vezes','death',2,'any','any','normal','classic',1,9,100,8),
(52,'Morra entre 20% e 29% duas vezes','death',2,'any','any','normal','classic',20,29,100,8),
(53,'Morra entre 40% e 49% duas vezes','death',2,'any','any','normal','classic',40,49,100,8),
(54,'Morra entre 60% e 69% duas vezes','death',2,'any','any','normal','classic',60,69,100,8),
(55,'Morra entre 80% e 89% duas vezes','death',2,'any','any','normal','classic',80,89,100,8),
(56,'Complete 3 levels classicos diferentes','complete',3,'any','any','normal','classic',0,100,175,8),
(57,'Complete 2 plataformas diferentes','complete',2,'any','any','normal','platformer',0,100,175,8),
(58,'Pratique 3 levels diferentes','complete',3,'any','any','practice','any',0,100,150,8),
(59,'Pratique 2 plataformas diferentes','complete',2,'any','any','practice','platformer',0,100,150,8),
(60,'Chegue a 50% em um Main Level','reach',1,'any','main','normal','classic',50,100,100,8),
(61,'Complete 3 Autos diferentes','complete',3,'auto','any','normal','any',0,100,150,8),
(62,'Complete 3 Easy diferentes','complete',3,'easy','any','normal','any',0,100,150,8),
(63,'Morra em 5 tentativas de plataforma','death',5,'any','any','normal','platformer',0,100,100,8),
(64,'Complete 3 levels online diferentes','complete',3,'any','online','normal','any',0,100,175,8)
) as v(sequence_order,title,kind,goal,difficulty,scope,mode,platformer,min_percent,max_percent,xp,tokens) on conflict do nothing;

insert into public.geobr_shop_items(id,name,kind,price,style,description,sort_order) values
('theme-fire','Fogo','theme',60,'fire','Chamas e tons de brasa no perfil.',0),
('theme-ice','Gelo','theme',60,'ice','Cristais e tons de gelo no perfil.',1),
('theme-forest','Floresta','theme',60,'forest','Folhagens e verde esmeralda.',2),
('theme-cosmos','Cosmos','theme',90,'cosmos','Estrelas sobre um ceu violeta.',3),
('theme-neon','Neon','theme',90,'neon','Luzes ciano e magenta.',4),
('theme-gold','Ouro','theme',150,'gold','Brilho dourado e formas preciosas.',5),
('title-iniciante','Iniciante','title',0,'','Titulo permanente para equipar no perfil.',6),
('title-antigao','Antigao','title',40,'','Titulo permanente para equipar no perfil.',7),
('title-rico','Rico','title',200,'','Titulo permanente para equipar no perfil.',8),
('title-explorador','Explorador','title',30,'','Titulo permanente para equipar no perfil.',9),
('title-persistente','Persistente','title',30,'','Titulo permanente para equipar no perfil.',10),
('title-colecionador','Colecionador','title',50,'','Titulo permanente para equipar no perfil.',11),
('title-veterano','Veterano','title',60,'','Titulo permanente para equipar no perfil.',12),
('title-lendario','Lendario','title',120,'','Titulo permanente para equipar no perfil.',13),
('title-mestre-do-dash','Mestre do Dash','title',100,'','Titulo permanente para equipar no perfil.',14),
('title-estrela-geobr','Estrela GeoBR','title',80,'','Titulo permanente para equipar no perfil.',15),
('skip-tier','Skip de etapa','tier_skip',80,'','Avanca uma etapa do passe. Nao aumenta o nivel pessoal.',16),
('skip-quest','Skip de quest','quest_skip',30,'','Troca a quest atual. Nao concede XP nem Tokens da quest.',17);

-- More milestones on the same path; existing tier claims stay untouched.
insert into public.geobr_pass_rewards(season_id,tier,kind,amount,icon_type,icon_id)
select s.id,t,case when t%5=0 then 'skin' when t%3=0 then 'stars' when t%2=0 then 'orbs' else 'tokens' end,
case when t%5=0 then 1 when t%3=0 then 2 when t%2=0 then 200 else 40 end,'cube',5+t/5
from public.geobr_pass_seasons s cross join generate_series(11,30) t on conflict do nothing;
create function geobr_private.seed_season_quests() returns trigger language plpgsql security definer set search_path='' as $$
declare source_id bigint;
begin
 select s.id into source_id from public.geobr_pass_seasons s where s.id<>new.id and exists(select 1 from public.geobr_quest_steps q where q.season_id=s.id) order by s.starts_at desc limit 1;
 if source_id is not null then
  insert into public.geobr_quest_steps(season_id,sequence_order,title,kind,goal,difficulty,scope,mode,platformer,min_percent,max_percent,xp,tokens,published)
  select new.id,sequence_order,title,kind,goal,difficulty,scope,mode,platformer,min_percent,max_percent,xp,tokens,published from public.geobr_quest_steps where season_id=source_id;
  insert into public.geobr_pass_rewards(season_id,tier,kind,amount,icon_type,icon_id)
  select new.id,tier,kind,amount,icon_type,icon_id from public.geobr_pass_rewards where season_id=source_id;
 end if;return new;
end $$;
revoke all on function geobr_private.seed_season_quests() from public,anon,authenticated;
create trigger geobr_seed_season_quests after insert on public.geobr_pass_seasons for each row execute function geobr_private.seed_season_quests();
create index geobr_quest_runs_season_idx on geobr_private.quest_runs(season_id);
create function geobr_private.reward_v2(p_reward_id bigint) returns jsonb language plpgsql security definer set search_path='' as $$
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));
 perform geobr_private.claim_reward(p_reward_id);return geobr_private.state_v2();
end $$;
create function public.geobr_reward_v2(p_reward_id bigint) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.reward_v2(p_reward_id)$$;
revoke all on function geobr_private.reward_v2(bigint),public.geobr_reward_v2(bigint) from public,anon;
grant execute on function geobr_private.reward_v2(bigint),public.geobr_reward_v2(bigint) to authenticated;
