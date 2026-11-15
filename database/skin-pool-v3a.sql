create function geobr_private.roll_skin() returns jsonb language plpgsql volatile security invoker set search_path='' as $$
declare n integer=1+floor(random()*8)::int;types text[]=array['cube','ship','ball','ufo','wave','robot','spider','swing'];base integer[]=array[50,30,20,20,20,10,10,10];span integer[]=array[50,30,20,20,20,20,20,20];
begin return jsonb_build_object('kind','skin','amount',1,'icon_type',types[n],'icon_id',base[n]+floor(random()*span[n])::int);end $$;
revoke all on function geobr_private.roll_skin() from public,anon,authenticated;

create or replace function geobr_private.progression_action(p_action text,p_payload jsonb) returns jsonb language plpgsql security definer set search_path='' as $$
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
