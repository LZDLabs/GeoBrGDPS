alter table public.geobr_shop_items add column acquisition text not null default 'shop' check(acquisition in ('shop','crate'));
alter table public.geobr_shop_items drop constraint geobr_shop_items_check;
alter table public.geobr_shop_items add constraint geobr_shop_items_check check(kind<>'theme' or style in ('fire','ice','forest','cosmos','neon','gold','aurora','eclipse','royal','astral'));
insert into public.geobr_shop_items(id,name,kind,price,style,description,sort_order,published,acquisition) values
('crate-theme-aurora','Aurora Boreal','theme',0,'aurora','Exclusivo de caixas. Cortinas de luz sobre montanhas.',100,true,'crate'),
('crate-theme-eclipse','Eclipse Rubro','theme',0,'eclipse','Exclusivo de caixas. Eclipse e horizonte carmesim.',101,true,'crate'),
('crate-theme-royal','Nebulosa Real','theme',0,'royal','Exclusivo de caixas. Orbitas e estrelas douradas.',102,true,'crate'),
('crate-theme-astral','Cristal Astral','theme',0,'astral','Exclusivo de caixas. Cristais e ceu gelado.',103,true,'crate'),
('crate-title-hunter','Cacador de Tesouros','title',0,'','Titulo exclusivo de caixas.',110,true,'crate'),
('crate-title-guardian','Guardiao do Cofre','title',0,'','Titulo exclusivo de caixas.',111,true,'crate'),
('crate-title-lucky','Sortudo','title',0,'','Titulo exclusivo de caixas.',112,true,'crate'),
('crate-title-relic','Reliquia Viva','title',0,'','Titulo exclusivo de caixas.',113,true,'crate');
create or replace function geobr_private.buy_item(p_item_id text,p_request_id uuid,p_expected_price integer,p_expected_run uuid default null,p_expected_tier integer default null)
returns jsonb language plpgsql security definer set search_path='' as $$
declare item public.geobr_shop_items; old geobr_private.shop_orders; sid bigint; per integer; xp bigint; run uuid; count integer;
begin
 if auth.uid() is null or p_request_id is null then raise exception 'Sessao e pedido necessarios'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));perform geobr_private.ensure_v2();
 if exists(select 1 from public.geobr_shop_items where id=p_item_id and acquisition='crate') then raise exception 'Item exclusivo de caixas: nao pode ser comprado';end if;
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
