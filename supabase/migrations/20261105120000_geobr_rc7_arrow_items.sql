-- GeoBR RC7 (1.9.0-beta.7) — Arrow skins as real items. Forward-only.

-- 1) Catalog: new item kind "arrow" and acquisition "pass".
alter table public.geobr_shop_items drop constraint if exists geobr_shop_items_kind_check;
alter table public.geobr_shop_items add constraint geobr_shop_items_kind_check
  check (kind = any (array['theme','title','tier_skip','quest_skip','arrow']));
alter table public.geobr_shop_items drop constraint if exists geobr_shop_items_acquisition_check;
alter table public.geobr_shop_items add constraint geobr_shop_items_acquisition_check
  check (acquisition = any (array['shop','crate','pass']));
alter table public.geobr_pass_rewards drop constraint if exists geobr_pass_rewards_kind_check;
alter table public.geobr_pass_rewards add constraint geobr_pass_rewards_kind_check
  check (kind = any (array['tokens','orbs','stars','skin','crate','quest_skip','theme','arrow']));

insert into public.geobr_shop_items(id,name,kind,price,style,description,sort_order,published,acquisition) values
 ('arrow-5','Arrow Raio','arrow',80,'5','Arrow em zigue-zague de raio.',200,true,'shop'),
 ('arrow-6','Arrow Tridente','arrow',120,'6','Tres pontas de ataque.',201,true,'shop'),
 ('arrow-7','Arrow Foguete','arrow',160,'7','Arrow com janela e aletas de foguete.',202,true,'shop'),
 ('arrow-8','Arrow Origami','arrow',200,'8','Aviao de papel dobrado.',203,true,'shop'),
 ('arrow-9','Arrow Bandeira','arrow',0,'9','Recompensa bonus do GeoPass.',210,true,'pass'),
 ('arrow-10','Arrow Coroa','arrow',0,'10','Recompensa bonus do GeoPass.',211,true,'pass'),
 ('arrow-11','Arrow Madeira','arrow',0,'11','Exclusiva da Caixa Comum.',220,true,'crate'),
 ('arrow-12','Arrow Esmeralda','arrow',0,'12','Exclusiva da Caixa Incomum.',221,true,'crate'),
 ('arrow-13','Arrow Safira','arrow',0,'13','Exclusiva da Caixa Rara.',222,true,'crate'),
 ('arrow-14','Arrow Ametista','arrow',0,'14','Exclusiva da Caixa Epica.',223,true,'crate'),
 ('arrow-15','Arrow Solar','arrow',0,'15','Exclusiva da Caixa Lendaria.',224,true,'crate'),
 ('arrow-16','Arrow Supernova','arrow',0,'16','Exclusiva da Caixa Mitica.',225,true,'crate')
on conflict (id) do nothing;

-- 2) Buying: pass items are not for sale; arrows are owned once (like themes).
do $mig$
declare d text;
begin
  select pg_get_functiondef('geobr_private.buy_item(text,uuid,integer,uuid,integer)'::regprocedure) into d;
  d=replace(d,'where id=p_item_id and acquisition=''crate'') then raise exception ''Item exclusivo de caixas: nao pode ser comprado'';',
              'where id=p_item_id and acquisition in (''crate'',''pass'')) then raise exception ''Item exclusivo de caixas ou do passe: nao pode ser comprado'';');
  d=replace(d,'if item.kind in (''theme'',''title'') and exists','if item.kind in (''theme'',''title'',''arrow'') and exists');
  d=replace(d,'if item.kind in (''theme'',''title'') then
  insert into public.geobr_inventory','if item.kind in (''theme'',''title'',''arrow'') then
  insert into public.geobr_inventory');
  if position('''arrow''' in d)=0 then raise exception 'buy_item patch did not apply'; end if;
  execute d;
end $mig$;

-- 3) Rewards (crates and pass) can deliver arrows; duplicates become Tokens.
do $mig$
declare d text;
begin
  select pg_get_functiondef('geobr_private.deliver(text,jsonb)'::regprocedure) into d;
  d=replace(d,'elsif k in (''theme'',''title'') then
  select * into item','elsif k in (''theme'',''title'',''arrow'') then
  select * into item');
  d=replace(d,'elsif k in (''theme'',''title'') then insert into public.geobr_inventory','elsif k in (''theme'',''title'',''arrow'') then insert into public.geobr_inventory');
  if (length(d)-length(replace(d,'''arrow''','')))/length('''arrow''')<>2 then raise exception 'deliver patch did not apply'; end if;
  execute d;

  select pg_get_functiondef('geobr_private.claim_reward(bigint)'::regprocedure) into d;
  d=replace(d,'elsif r.kind=''theme'' then insert into public.geobr_inventory','elsif r.kind in (''theme'',''arrow'') then insert into public.geobr_inventory');
  if position('''arrow''' in d)=0 then raise exception 'claim_reward patch did not apply'; end if;
  execute d;
end $mig$;

-- 4) Each crate rarity has its own exclusive Arrow (slot 3, chance grows with rarity).
create or replace function geobr_private.crate_arrow(p_rarity integer)
 returns jsonb
 language sql
 set search_path to ''
as $function$
 select jsonb_build_object('kind','arrow','amount',1,'item_id','arrow-'||(10+least(greatest(p_rarity,1),6)))
$function$;
revoke all on function geobr_private.crate_arrow(integer) from public, anon, authenticated;

do $mig$
declare d text;
begin
  select pg_get_functiondef('geobr_private.open_crate_v4(uuid)'::regprocedure) into d;
  d=replace(d,'  else
   roll=random();',
'  elsif i=3 and random()<(array[.08,.10,.12,.15,.20,.30])[c.rarity] then r=geobr_private.crate_arrow(c.rarity);
  else
   roll=random();');
  if position('crate_arrow' in d)=0 then raise exception 'open_crate_v4 patch did not apply'; end if;
  execute d;
end $mig$;

-- 5) Equipped flag for arrows in the player state.
do $mig$
declare d text;
begin
  select pg_get_functiondef('geobr_private.state_v2_legacy()'::regprocedure) into d;
  d=replace(d,'when kind=''title'' then i.name=profile.title_name else false end',
              'when kind=''title'' then i.name=profile.title_name when kind=''arrow'' then i.style=profile.arrow_skin::text else false end');
  if position('profile.arrow_skin' in d)=0 then raise exception 'state_v2_legacy patch did not apply'; end if;
  execute d;
end $mig$;

-- 6) Equipping validates ownership: skins 1-4 are free, others need the item.
create or replace function geobr_private.profile_arrow(p_skin integer, p_glow boolean)
 returns jsonb
 language plpgsql
 security definer
 set search_path to ''
as $function$
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 perform geobr_private.require_access('mod');
 if p_skin is null or p_skin not between 1 and 16 or p_glow is null then raise exception 'Skin invalida'; end if;
 if p_skin>4 and not exists(select 1 from public.geobr_inventory where user_id=auth.uid() and item_id='arrow-'||p_skin) then
  raise exception 'Voce ainda nao tem esta skin do Arrow';
 end if;
 perform geobr_private.ensure_v2();
 update public.geobr_profiles set arrow_skin=p_skin,arrow_glow=p_glow,updated_at=now()
  where user_id=auth.uid() and (arrow_skin,arrow_glow) is distinct from (p_skin::smallint,p_glow);
 return jsonb_build_object('ok',true,'arrow_skin',p_skin,'arrow_glow',p_glow);
end $function$;
alter table public.geobr_profiles drop constraint if exists geobr_profiles_arrow_skin_check;
alter table public.geobr_profiles add constraint geobr_profiles_arrow_skin_check check (arrow_skin between 1 and 16);

-- 7) GeoPass: bonus tiers 31 and 32 of the active season award the pass arrows.
insert into public.geobr_pass_rewards(season_id,tier,kind,amount,item_id)
select s.id, t.tier, 'arrow', 1, t.item
from public.geobr_pass_seasons s
cross join (values (31,'arrow-9'),(32,'arrow-10')) t(tier,item)
where s.published and s.starts_at<=now() and s.ends_at>now()
on conflict (season_id,tier) do nothing;

-- 8) RC7 release row.
insert into geobr_private.releases(version, enabled, message)
values ('1.9.0-beta.7', true, 'GeoBR RC7')
on conflict (version) do nothing;
