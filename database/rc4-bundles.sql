create or replace function geobr_private.deliver(p_key text,p_reward jsonb) returns jsonb language plpgsql security definer set search_path='' as $$
declare r jsonb=p_reward;k text=r->>'kind';a integer=coalesce((r->>'amount')::int,1);item public.geobr_shop_items;previous jsonb;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria';end if;
 select reward into previous from public.geobr_reward_claims where user_id=auth.uid() and source_key=p_key;
 if found then return previous;end if;
 if k='skin' and exists(select 1 from public.geobr_reward_claims where user_id=auth.uid() and reward->>'kind'='skin' and reward->>'icon_type'=r->>'icon_type' and reward->>'icon_id'=r->>'icon_id') then
  r=jsonb_build_object('kind','tokens','amount',100,'duplicate',true);k='tokens';a=100;
 elsif k in ('theme','title') then
  select * into item from public.geobr_shop_items where id=r->>'item_id' and kind=k;
  if not found then raise exception 'Cosmetico indisponivel';end if;
  if exists(select 1 from public.geobr_inventory where user_id=auth.uid() and item_id=item.id) then
   r=jsonb_build_object('kind','tokens','amount',100,'duplicate',true);k='tokens';a=100;
  else r=r||jsonb_build_object('name',item.name,'style',item.style,'exclusive',item.acquisition='crate');end if;
 end if;
 insert into public.geobr_reward_claims(user_id,reward,source_key) values(auth.uid(),r,p_key);
 if k='tokens' then update public.geobr_wallets set tokens=tokens+a where user_id=auth.uid();
 elsif k='crate' then insert into public.geobr_crates(user_id,rarity,source) values(auth.uid(),(r->>'crate_rarity')::int,p_key) on conflict do nothing;
 elsif k='quest_skip' then update geobr_private.progression set skips=skips+a where user_id=auth.uid();
 elsif k in ('theme','title') then insert into public.geobr_inventory(user_id,item_id) values(auth.uid(),item.id) on conflict do nothing;
 end if;
 return r;
end $$;
create function geobr_private.crate_cosmetic(p_kind text) returns jsonb language sql volatile security invoker set search_path='' as $$
 select jsonb_build_object('kind',kind,'amount',1,'item_id',id) from public.geobr_shop_items where acquisition='crate' and kind=p_kind order by random() limit 1
$$;
create function geobr_private.open_crate_v4(p_id uuid) returns jsonb language plpgsql security definer set search_path='' as $$
declare c public.geobr_crates;items jsonb='[]';r jsonb;n integer;i integer;roll float;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria';end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));perform geobr_private.ensure_v2();
 insert into geobr_private.progression(user_id) values(auth.uid()) on conflict do nothing;
 select * into c from public.geobr_crates where id=p_id and user_id=auth.uid() for update;
 if not found then raise exception 'Caixa nao encontrada';end if;
 if c.opened_at is not null then
  return jsonb_build_object('state',geobr_private.state_v2(),'reward',c.reward,'items',case when c.reward->>'kind'='bundle' then c.reward->'items' else jsonb_build_array(c.reward) end);
 end if;
 n=(array[4,5,6,7,8,10])[c.rarity];
 for i in 1..n loop
  if i=1 then r=jsonb_build_object('kind','tokens','amount',20*c.rarity*c.rarity);
  elsif i=2 then r=jsonb_build_object('kind','orbs','amount',100*c.rarity*c.rarity);
  elsif (c.rarity>=3 and i=n) or (c.rarity=6 and i=n-2) then r=geobr_private.roll_skin();
  elsif c.rarity>=4 and i=n-1 then r=geobr_private.crate_cosmetic(case when random()<.5 then 'title' else 'theme' end);
  else
   roll=random();
   if roll<.35 then r=jsonb_build_object('kind','tokens','amount',10*c.rarity);
   elsif roll<.65 then r=jsonb_build_object('kind','orbs','amount',50*c.rarity);
   elsif roll<.80 then r=jsonb_build_object('kind','quest_skip','amount',greatest(1,c.rarity/2));
   elsif roll<.92 then r=geobr_private.roll_skin();
   else r=geobr_private.crate_cosmetic(case when roll<.96 then 'title' else 'theme' end);end if;
  end if;
  r=geobr_private.deliver('crate:'||c.id||':slot:'||i,r);items=items||jsonb_build_array(r);
 end loop;
 r=jsonb_build_object('kind','bundle','items',items,'amount',n,'version',4);
 update public.geobr_crates set opened_at=now(),reward=r where id=c.id;
 return jsonb_build_object('state',geobr_private.state_v2(),'reward',r,'items',items);
end $$;
create function public.geobr_open_crate_v4(p_id uuid) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.open_crate_v4(p_id)$$;
-- One network roundtrip for a mission event and its possible completion drop.
create function geobr_private.activity_v4(p_event jsonb) returns jsonb language plpgsql security definer set search_path='' as $$
declare s jsonb;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria';end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));
 if coalesce(p_event->>'run','')<>'' then s=geobr_private.record_activity(p_event);else s=geobr_private.state_v2();end if;
 if p_event->>'kind'='complete' and p_event->>'mode'='normal' and coalesce((p_event->>'elapsed')::float,0)>=15 then return geobr_private.progression_action('drop',p_event);end if;
 return jsonb_build_object('state',s);
end $$;
create function public.geobr_activity_v4(p_event jsonb) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.activity_v4(p_event)$$;
revoke all on function geobr_private.crate_cosmetic(text),geobr_private.open_crate_v4(uuid),public.geobr_open_crate_v4(uuid),geobr_private.activity_v4(jsonb),public.geobr_activity_v4(jsonb) from public,anon,authenticated;
grant execute on function geobr_private.open_crate_v4(uuid),public.geobr_open_crate_v4(uuid),geobr_private.activity_v4(jsonb),public.geobr_activity_v4(jsonb) to authenticated;
notify pgrst,'reload schema';
