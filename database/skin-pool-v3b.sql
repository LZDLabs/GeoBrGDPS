create or replace function geobr_private.season_builder(p_action text,p_payload jsonb) returns jsonb language plpgsql security definer set search_path='' as $$
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
