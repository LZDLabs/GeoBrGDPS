-- Additive progression, authoritative reward amounts and serialized economy.
alter table public.geobr_reward_claims alter column reward_id drop not null;
alter table public.geobr_reward_claims add column source_key text;
create unique index geobr_reward_source on public.geobr_reward_claims(user_id,source_key);
alter table public.geobr_pass_rewards drop constraint geobr_pass_rewards_kind_check;
alter table public.geobr_pass_rewards add constraint geobr_pass_rewards_kind_check check(kind in ('tokens','orbs','stars','skin','crate','quest_skip','theme'));
alter table public.geobr_pass_rewards add column crate_rarity integer not null default 1 check(crate_rarity between 1 and 6);
alter table public.geobr_pass_rewards add column bonus_crate integer not null default 0 check(bonus_crate between 0 and 6);
alter table public.geobr_pass_rewards add column item_id text references public.geobr_shop_items(id);
create table geobr_private.progression (
 user_id uuid primary key references auth.users(id) on delete cascade,
 claimed_level integer not null default 1, skips integer not null default 0 check(skips>=0)
);
create table public.geobr_crates (
 id uuid primary key default gen_random_uuid(), user_id uuid not null references auth.users(id) on delete cascade,
 rarity integer not null check(rarity between 1 and 6),source text not null, opened_at timestamptz,reward jsonb,created_at timestamptz not null default now(),unique(user_id,source)
);
alter table public.geobr_crates enable row level security;
revoke all on public.geobr_crates from public,anon,authenticated;
grant select on public.geobr_crates to authenticated;
create policy crates_own on public.geobr_crates for select to authenticated using(user_id=(select auth.uid()));
create index crates_owner on public.geobr_crates(user_id,created_at desc);
create table geobr_private.drop_rolls (
 user_id uuid references auth.users(id) on delete cascade,day date not null,level_key text not null,
 event_id uuid not null,created_at timestamptz not null default now(),primary key(user_id,day,level_key),unique(user_id,event_id)
);
create table geobr_private.season_drafts (
 id uuid primary key default gen_random_uuid(),owner uuid not null references auth.users(id) on delete cascade,
 payload jsonb not null, published_season bigint references public.geobr_pass_seasons(id),created_at timestamptz not null default now()
);
alter table geobr_private.progression enable row level security;
alter table geobr_private.drop_rolls enable row level security;
alter table geobr_private.season_drafts enable row level security;
revoke all on geobr_private.progression,geobr_private.drop_rolls,geobr_private.season_drafts from public,anon,authenticated;

create function geobr_private.roll_skin() returns jsonb language plpgsql volatile security invoker set search_path='' as $$
declare n integer=1+floor(random()*8)::int;types text[]=array['cube','ship','ball','ufo','wave','robot','spider','swing'];base integer[]=array[50,30,20,20,20,10,10,10];span integer[]=array[50,30,20,20,20,20,20,20];
begin return jsonb_build_object('kind','skin','amount',1,'icon_type',types[n],'icon_id',base[n]+floor(random()*span[n])::int);end $$;
revoke all on function geobr_private.roll_skin() from public,anon,authenticated;

create function geobr_private.deliver(p_key text,p_reward jsonb) returns jsonb language plpgsql security definer set search_path='' as $$
declare r jsonb=p_reward; k text=p_reward->>'kind'; a integer=coalesce((p_reward->>'amount')::int,1); inserted integer; item text;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria';end if;
 -- Private helper, caller owns the economy lock and chooses the reward. Never exposed.
 if k='skin' and exists(select 1 from public.geobr_reward_claims where user_id=auth.uid() and reward->>'kind'='skin' and reward->>'icon_type'=r->>'icon_type' and reward->>'icon_id'=r->>'icon_id') then
  r=jsonb_build_object('kind','tokens','amount',100,'duplicate',true);k='tokens';a=100;
 elsif k='theme' then
  item=r->>'item_id';
  if not exists(select 1 from public.geobr_shop_items where id=item and kind='theme') then raise exception 'Tema indisponivel';end if;
  if exists(select 1 from public.geobr_inventory where user_id=auth.uid() and item_id=item) then r=jsonb_build_object('kind','tokens','amount',100,'duplicate',true);k='tokens';a=100;end if;
 end if;
 insert into public.geobr_reward_claims(user_id,reward,source_key) values(auth.uid(),r,p_key) on conflict(user_id,source_key) do nothing;
 get diagnostics inserted=row_count;
 if inserted=0 then return (select reward from public.geobr_reward_claims where user_id=auth.uid() and source_key=p_key);end if;
 if k='tokens' then update public.geobr_wallets set tokens=tokens+a where user_id=auth.uid();
 elsif k='crate' then insert into public.geobr_crates(user_id,rarity,source) values(auth.uid(),(r->>'crate_rarity')::int,p_key) on conflict do nothing;
 elsif k='quest_skip' then update geobr_private.progression set skips=skips+a where user_id=auth.uid();
 elsif k='theme' then insert into public.geobr_inventory(user_id,item_id) values(auth.uid(),item) on conflict do nothing;
 end if;
 return r;
end $$;
create function geobr_private.level_prize(p_level integer) returns jsonb language sql immutable set search_path='' as $$
 select case when p_level%25=0 then jsonb_build_object('kind','crate','amount',1,'crate_rarity',6)
 when p_level%10=0 then jsonb_build_object('kind','crate','amount',1,'crate_rarity',5)
 when p_level%5=0 then jsonb_build_object('kind','crate','amount',1,'crate_rarity',4)
 when p_level%3=0 then jsonb_build_object('kind','orbs','amount',100+10*p_level)
 else jsonb_build_object('kind','tokens','amount',15+5*least(p_level,100)) end
$$;
-- Preserve the established state and all callers, extending rather than replacing old progress.
alter function geobr_private.state_v2() rename to state_v2_legacy;
create function geobr_private.state_v2() returns jsonb language plpgsql security definer set search_path='' as $$
declare b jsonb; meta geobr_private.progression; lv integer;
begin
 b=geobr_private.state_v2_legacy();
 insert into geobr_private.progression(user_id) values(auth.uid()) on conflict do nothing;
 select * into meta from geobr_private.progression where user_id=auth.uid();lv=(b->'profile'->>'level')::int;
 return b||jsonb_build_object('skips',meta.skips,'claimed_level',meta.claimed_level,
 'level_rewards',(select jsonb_agg(jsonb_build_object('level',n,'reward',geobr_private.level_prize(n),'ready',n<=lv)) from generate_series(meta.claimed_level+1,meta.claimed_level+6) n),
 'crates',coalesce((select jsonb_agg(to_jsonb(c)-'user_id' order by c.created_at) from public.geobr_crates c where user_id=auth.uid() and opened_at is null),'[]'::jsonb));
end $$;
create or replace function geobr_private.claim_reward(p_reward_id bigint) returns jsonb language plpgsql security definer set search_path='' as $$
declare r public.geobr_pass_rewards; threshold integer; inserted integer;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria';end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));
 perform geobr_private.ensure_v2();insert into geobr_private.progression(user_id) values(auth.uid()) on conflict do nothing;
 select * into r from public.geobr_pass_rewards where id=p_reward_id for share;
 select xp_per_tier*r.tier into threshold from public.geobr_pass_seasons where id=r.season_id and published and starts_at<=now() and ends_at>now();
 if threshold is null or coalesce((select xp from public.geobr_pass_progress where user_id=auth.uid() and season_id=r.season_id),0)<threshold then raise exception 'XP insuficiente ou temporada encerrada';end if;
 -- Keep original reward_id receipts for pass-state and immutable catalog protection.
 insert into public.geobr_reward_claims(user_id,reward_id,reward) values(auth.uid(),r.id,to_jsonb(r)) on conflict(user_id,reward_id) do nothing;
 get diagnostics inserted=row_count;
 if inserted>0 then
  if r.kind='tokens' then update public.geobr_wallets set tokens=tokens+r.amount where user_id=auth.uid();
  elsif r.kind='crate' then insert into public.geobr_crates(user_id,rarity,source) values(auth.uid(),r.crate_rarity,'pass:'||r.id);
  elsif r.kind='quest_skip' then update geobr_private.progression set skips=skips+r.amount where user_id=auth.uid();
  elsif r.kind='theme' then insert into public.geobr_inventory(user_id,item_id) values(auth.uid(),r.item_id) on conflict do nothing;end if;
  if r.bonus_crate>0 then insert into public.geobr_crates(user_id,rarity,source) values(auth.uid(),r.bonus_crate,'bonus:'||r.id) on conflict do nothing;end if;
 end if;
 return geobr_private.pass_state();
end $$;

create function geobr_private.progression_action(p_action text,p_payload jsonb) returns jsonb language plpgsql security definer set search_path='' as $$
declare u uuid=auth.uid(); c public.geobr_crates; r jsonb; roll float; lv integer; claimed integer; rarity integer; chance float;
 diff text; levelkey text; v_day date=(now() at time zone 'America/Sao_Paulo')::date; inserted integer; attempts integer;
begin
 if u is null then raise exception 'Sessao necessaria';end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||u::text,0));perform geobr_private.ensure_v2();
 insert into geobr_private.progression(user_id) values(u) on conflict do nothing;
 if p_action='level' then
  select floor(sqrt(lifetime_xp::numeric/100))::int+1 into lv from public.geobr_profiles where user_id=u;
  select claimed_level into claimed from geobr_private.progression where user_id=u for update;
  if (p_payload->>'level')::int is null then raise exception 'Nivel necessario';end if;
  if (p_payload->>'level')::int<=claimed then return jsonb_build_object('state',geobr_private.state_v2(),'reward',(select reward from public.geobr_reward_claims where user_id=u and source_key='level:'||(p_payload->>'level')));end if;
  if claimed>=lv or (p_payload->>'level')::int<>claimed+1 then raise exception 'Ganhe XP para subir de nivel';end if;
  r=geobr_private.deliver('level:'||(claimed+1),geobr_private.level_prize(claimed+1));
  update geobr_private.progression set claimed_level=claimed+1 where user_id=u;
 elsif p_action='skip' then
  if (select run_id::text from geobr_private.quest_runs where user_id=u and season_id=(select id from public.geobr_pass_seasons where published and starts_at<=now() and ends_at>now() limit 1)) is distinct from (p_payload->>'run') then raise exception 'Missao mudou. Atualize.';end if;
  update geobr_private.progression set skips=skips-1 where user_id=u and skips>0;
  get diagnostics inserted=row_count;if inserted=0 then raise exception 'Voce nao possui Skips';end if;
  perform geobr_private.advance_quest((select id from public.geobr_pass_seasons where published and starts_at<=now() and ends_at>now() limit 1),true);
 elsif p_action='open' then
  select * into c from public.geobr_crates where id=(p_payload->>'id')::uuid and user_id=u for update;
  if not found then raise exception 'Caixa nao encontrada';end if;
  if c.opened_at is not null then return jsonb_build_object('state',geobr_private.state_v2(),'reward',c.reward);end if;
  roll=random();rarity=c.rarity;
  -- Exclusive, documented category probabilities; quantity scales by rarity.
  if roll<.40 then r=jsonb_build_object('kind','tokens','amount',20*rarity*rarity);
  elsif roll<.70 then r=jsonb_build_object('kind','orbs','amount',100*rarity*rarity);
  elsif roll<.80 then r=jsonb_build_object('kind','quest_skip','amount',greatest(1,rarity/2));
  elsif roll<.95 then r=geobr_private.roll_skin();
  else r=jsonb_build_object('kind','theme','amount',1,'item_id',(array['theme-fire','theme-ice','theme-forest','theme-cosmos','theme-neon','theme-gold'])[rarity]);end if;
  r=geobr_private.deliver('crate:'||c.id,r);
  update public.geobr_crates set opened_at=now(),reward=r where id=c.id;
 elsif p_action='drop' then
  levelkey=p_payload->>'level';diff=p_payload->>'difficulty';
  if p_payload->>'kind'<>'complete' or p_payload->>'mode'<>'normal' or coalesce((p_payload->>'percent')::int,0)<>100 or coalesce((p_payload->>'elapsed')::float,0)<15 then return jsonb_build_object('state',geobr_private.state_v2());end if;
  if levelkey is null or levelkey !~ '^(main|online|official):[1-9][0-9]{0,9}$' then raise exception 'Nivel invalido';end if;
  if split_part(levelkey,':',1)='main' then
   select case when lower(difficulty) like '%demon%' then 'demon' else lower(difficulty) end into diff from public.geobr_main_levels where id=split_part(levelkey,':',2)::bigint and published;
  end if;
  chance=case diff when 'easy' then .10 when 'normal' then .15 when 'hard' then .20 when 'harder' then .25 when 'insane' then .35 when 'demon' then .45 else 0 end;
  if chance=0 then return jsonb_build_object('state',geobr_private.state_v2());end if;
  select count(*) into attempts from geobr_private.drop_rolls where user_id=u and drop_rolls.day=v_day;
  if attempts>=6 or exists(select 1 from geobr_private.drop_rolls where user_id=u and created_at>now()-interval '30 seconds') then return jsonb_build_object('state',geobr_private.state_v2());end if;
  insert into geobr_private.drop_rolls(user_id,day,level_key,event_id) values(u,v_day,levelkey,(p_payload->>'id')::uuid) on conflict do nothing;
  get diagnostics inserted=row_count;
  if inserted>0 and random()<chance then
   roll=random();rarity=case when roll<.60 then 1 when roll<.85 then 2 when roll<.95 then 3 when roll<.99 then 4 when roll<.999 then 5 else 6 end;
   insert into public.geobr_crates(user_id,rarity,source) values(u,rarity,'drop:'||v_day||':'||levelkey) returning * into c;
  end if;
 else raise exception 'Acao desconhecida';end if;
 return jsonb_build_object('state',geobr_private.state_v2(),'reward',r,'drop',case when c.id is not null and p_action='drop' then to_jsonb(c)-'user_id' else null end);
end $$;
create function public.geobr_progression(p_action text,p_payload jsonb default '{}'::jsonb) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.progression_action(p_action,p_payload)$$;

-- Rank writes use an explicit authenticated RPC and return the saved row.
create function geobr_private.save_rank(p_level bigint,p_position integer,p_published boolean) returns jsonb language plpgsql security definer set search_path='' as $$
declare r public.geobr_level_ranks;
begin
 if auth.uid() is null or not geobr_private.is_admin() then raise exception 'Sessao ADM expirada. Entre novamente.';end if;
 if p_level is null or p_level<=0 or p_level>=1500000000 or p_position is null or p_position<1 then raise exception 'ID e TOP devem ser maiores que zero';end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-ranks',0));
 if exists(select 1 from public.geobr_level_ranks where position=p_position and level_id<>p_level) then raise exception 'Posicao TOP ocupada. Edite ou remova o registro existente.';end if;
 insert into public.geobr_level_ranks(level_id,position,published) values(p_level,p_position,p_published) on conflict(level_id) do update set position=excluded.position,published=excluded.published returning * into r;
 return to_jsonb(r);
end $$;
create function public.geobr_save_rank(p_level bigint,p_position integer,p_published boolean) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.save_rank(p_level,p_position,p_published)$$;

-- Add bonus crates without modifying rewards already claimed by anyone.
update public.geobr_pass_rewards r set bonus_crate=case when tier%30=0 then 6 when tier%10=0 then 4 else 2 end
where tier%5=0 and not exists(select 1 from public.geobr_reward_claims c where c.reward_id=r.id)
and exists(select 1 from public.geobr_pass_seasons s where s.id=r.season_id and s.published and s.ends_at>now());

revoke all on function geobr_private.deliver(text,jsonb),geobr_private.level_prize(integer),geobr_private.state_v2_legacy(),geobr_private.progression_action(text,jsonb),geobr_private.save_rank(bigint,integer,boolean),geobr_private.state_v2(),public.geobr_progression(text,jsonb),public.geobr_save_rank(bigint,integer,boolean) from public,anon,authenticated;
grant execute on function geobr_private.state_v2(),geobr_private.progression_action(text,jsonb),geobr_private.save_rank(bigint,integer,boolean),public.geobr_progression(text,jsonb),public.geobr_save_rank(bigint,integer,boolean) to authenticated;
create function geobr_private.season_builder(p_action text,p_payload jsonb) returns jsonb language plpgsql security definer set search_path='' as $$
declare d geobr_private.season_drafts; cfg jsonb; rewards jsonb='[]'; quests jsonb='[]'; r jsonb; q jsonb;
 n integer; tiers integer; days integer; countq integer; per integer; goal integer; k text; diff text; lo integer; sid bigint; starts timestamptz; ends timestamptz;
begin
 if auth.uid() is null or not geobr_private.is_admin() then raise exception 'Sessao ADM expirada';end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-season-builder',0));
 if p_action='generate' then
  tiers=(p_payload->>'tiers')::int;days=(p_payload->>'days')::int;countq=(p_payload->>'quests')::int;per=(p_payload->>'xp_per_tier')::int;
  if tiers is null or tiers not between 5 and 100 or days is null or days not between 1 and 180 or countq is null or countq not between 5 and 100 or per is null or per not between 50 and 1000 or coalesce(length(btrim(p_payload->>'name')),0) not between 1 and 60 then raise exception 'Use 5-100 etapas, 1-180 dias, 5-100 missoes e 50-1000 XP/etapa';end if;
  select greatest(now()+interval '5 minutes',coalesce(max(ends_at),now())) into starts from public.geobr_pass_seasons where published;
  ends=starts+make_interval(days=>days);
  for n in 1..tiers loop
   if n=tiers or n%5=0 then r=jsonb_build_object('kind','crate','amount',1,'crate_rarity',case when n=tiers then 6 when n%10=0 then 4 else 2 end);
   elsif random()<.25 then r=geobr_private.roll_skin();
   elsif random()<.4 then r=jsonb_build_object('kind','orbs','amount',100+50*(n/5));
   else r=jsonb_build_object('kind','tokens','amount',20+5*(n/5));end if;
   rewards=rewards||jsonb_build_array(r||jsonb_build_object('tier',n));
  end loop;
  for n in 1..countq loop
   k=case when random()<.3 then 'reach' else 'complete' end;goal=1+floor(random()*3)::int;
   diff=case when random()<.7 then 'any' else (array['easy','normal','hard','harder'])[1+floor(random()*4)::int] end;
   lo=case when k='reach' then (array[25,50,75])[1+floor(random()*3)::int] else 100 end;
   q=jsonb_build_object('sequence_order',n,'kind',k,'goal',goal,'difficulty',diff,'min_percent',lo,'xp',per,'tokens',8+goal*2,
    'title',case when k='reach' then 'Alcance '||lo||'% em ' else 'Complete ' end||goal||' niveis'||case when diff='any' then ' diferentes' else ' '||diff end);
   quests=quests||jsonb_build_array(q);
  end loop;
  cfg=jsonb_build_object('name',btrim(p_payload->>'name'),'starts_at',starts,'ends_at',ends,'xp_per_tier',per,'rewards',rewards,'quests',quests);
  insert into geobr_private.season_drafts(owner,payload) values(auth.uid(),cfg) returning * into d;
 elsif p_action='preview' then
  select * into d from geobr_private.season_drafts where owner=auth.uid() and published_season is null order by created_at desc limit 1;
  if not found then raise exception 'Gere uma previa primeiro';end if;
 elsif p_action='publish' then
  select * into d from geobr_private.season_drafts where id=(p_payload->>'id')::uuid and owner=auth.uid() for update;
  if not found then raise exception 'Previa nao encontrada';end if;
  if d.published_season is not null then return jsonb_build_object('season_id',d.published_season,'published',true);end if;
  cfg=d.payload;starts=(cfg->>'starts_at')::timestamptz;ends=(cfg->>'ends_at')::timestamptz;
  if starts<=now() or exists(select 1 from public.geobr_pass_seasons where published and starts_at<ends and ends_at>starts) then raise exception 'Datas conflitantes. Gere uma nova previa.';end if;
  insert into public.geobr_pass_seasons(name,starts_at,ends_at,xp_per_tier,published) values(cfg->>'name',starts,ends,(cfg->>'xp_per_tier')::int,false) returning id into sid;
  -- Newly inserted season only; remove automatic copies before applying the reviewed draft.
  delete from public.geobr_quest_steps where season_id=sid;delete from public.geobr_pass_rewards where season_id=sid;
  for r in select value from jsonb_array_elements(cfg->'rewards') loop
   insert into public.geobr_pass_rewards(season_id,tier,kind,amount,icon_type,icon_id,crate_rarity) values(sid,(r->>'tier')::int,r->>'kind',(r->>'amount')::int,coalesce(r->>'icon_type','cube'),coalesce((r->>'icon_id')::int,1),coalesce((r->>'crate_rarity')::int,1));
  end loop;
  for q in select value from jsonb_array_elements(cfg->'quests') loop
   insert into public.geobr_quest_steps(season_id,sequence_order,title,kind,goal,difficulty,scope,mode,platformer,min_percent,max_percent,xp,tokens,published)
   values(sid,(q->>'sequence_order')::int,q->>'title',q->>'kind',(q->>'goal')::int,q->>'difficulty','any','normal','any',(q->>'min_percent')::int,100,(q->>'xp')::int,(q->>'tokens')::int,true);
  end loop;
  update public.geobr_pass_seasons set published=true where id=sid;
  update geobr_private.season_drafts set published_season=sid where id=d.id;
  return jsonb_build_object('season_id',sid,'published',true);
 else raise exception 'Acao desconhecida';end if;
 return jsonb_build_object('id',d.id,'config',d.payload);
end $$;
create function public.geobr_season_builder(p_action text,p_payload jsonb default '{}'::jsonb) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.season_builder(p_action,p_payload)$$;
revoke all on function geobr_private.season_builder(text,jsonb),public.geobr_season_builder(text,jsonb) from public,anon;
grant execute on function geobr_private.season_builder(text,jsonb),public.geobr_season_builder(text,jsonb) to authenticated;
notify pgrst,'reload schema';
