-- GeoBR Natal 2026, part 3: Loja de Doces (daily accessory offers bought with
-- Candy, the same for everyone, renewed at midnight in Brasilia) and the owner's
-- "unlock every accessory" for testing. Non-destructive.

alter table geobr_private.natal_config add column if not exists accessory_prices jsonb not null
 default '{"comum":120,"incomum":220,"raro":400,"epico":750,"lendario":1300,"extremamente_raro":2200}';
alter table geobr_private.natal_config add column if not exists shop_size int not null default 4;

create or replace function geobr_private.natal_shop_offers(p_day date) returns setof public.geobr_natal_accessories
language sql stable security definer set search_path = '' as $$
 select a.* from public.geobr_natal_accessories a
 where a.published and a.source = 'caixa'
 order by md5(p_day::text || a.id)
 limit (select greatest(1, least(8, shop_size)) from geobr_private.natal_config where id = 1) $$;

create or replace function geobr_private.natal_shop() returns jsonb language plpgsql security definer set search_path = '' as $$
declare today date = geobr_private.natal_today(); prices jsonb;
begin
 perform geobr_private.require_access();
 select accessory_prices into prices from geobr_private.natal_config where id = 1;
 return jsonb_build_object('day', today, 'now', now(),
  'resets_at', (((now() at time zone 'America/Sao_Paulo')::date + 1)::timestamp at time zone 'America/Sao_Paulo'),
  'box_prices', (select box_prices from geobr_private.natal_config where id = 1),
  'offers', coalesce((select jsonb_agg(jsonb_build_object('id', o.id, 'name', o.name, 'rarity', o.rarity, 'sprite', o.sprite,
     'price', coalesce((prices ->> o.rarity)::int, 500),
     'owned', exists (select 1 from geobr_private.natal_inventory i where i.user_id = auth.uid() and i.accessory_id = o.id)))
   from geobr_private.natal_shop_offers(today) o), '[]'::jsonb));
end $$;

create or replace function geobr_private.natal_buy(p_accessory text) returns jsonb language plpgsql security definer set search_path = '' as $$
declare p geobr_private.natal_players; a public.geobr_natal_accessories; price int; ref text = gen_random_uuid()::text;
begin
 p = geobr_private.natal_me();
 select o.* into a from geobr_private.natal_shop_offers(geobr_private.natal_today()) o where o.id = p_accessory;
 if a.id is null then raise exception 'Esse acessorio nao esta na loja hoje'; end if;
 if exists (select 1 from geobr_private.natal_inventory where user_id = auth.uid() and accessory_id = a.id) then raise exception 'Voce ja tem esse acessorio'; end if;
 price = coalesce(((select accessory_prices from geobr_private.natal_config where id = 1) ->> a.rarity)::int, 500);
 if p.candy < price then raise exception 'Candy insuficiente (precisa de %)', price; end if;
 perform geobr_private.natal_add_candy(auth.uid(), -price, 'loja', ref);
 insert into geobr_private.natal_inventory(user_id, accessory_id, source) values (auth.uid(), a.id, 'loja') on conflict do nothing;
 return jsonb_build_object('reward', jsonb_build_object('kind', 'accessory', 'id', a.id, 'name', a.name, 'rarity', a.rarity, 'duplicate', false),
  'state', geobr_private.natal_state(), 'shop', geobr_private.natal_shop());
end $$;

-- Staff only: gives every published accessory to a player (empty name = me). Logged.
create or replace function geobr_private.natal_admin_unlock(p_player text) returns jsonb language plpgsql security definer set search_path = '' as $$
declare target uuid; n int;
begin
 if not geobr_private.natal_staff() then raise exception 'So o Dono e a staff de eventos usam esse painel'; end if;
 if nullif(trim(coalesce(p_player, '')), '') is null then target = auth.uid();
 else select supabase_user into target from geobr_gdps.accounts where lower(username) = lower(trim(p_player)) limit 1;
 end if;
 if target is null then raise exception 'Jogador nao encontrado (ou sem conta GeoBR vinculada)'; end if;
 insert into geobr_private.natal_players(user_id) values (target) on conflict do nothing;
 insert into geobr_private.natal_inventory(user_id, accessory_id, source)
  select target, id, 'dono' from public.geobr_natal_accessories where published on conflict do nothing;
 get diagnostics n = row_count;
 insert into geobr_private.natal_admin_log(admin, action, data) values (auth.uid(), 'unlock_all', jsonb_build_object('player', coalesce(p_player, ''), 'added', n));
 return jsonb_build_object('added', n, 'state', case when target = auth.uid() then geobr_private.natal_state() end);
end $$;

do $$
declare f record;
begin
 for f in select * from (values ('natal_shop', ''), ('natal_buy', 'p_accessory text'), ('natal_admin_unlock', 'p_player text')) as t(name, sig)
 loop
  execute format('revoke all on function geobr_private.%I(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function geobr_private.%I(%s) to authenticated', f.name, f.sig);
  execute format('create or replace function public.geobr_%s(%s) returns jsonb language sql set search_path = '''' as $f$ select geobr_private.%I(%s) $f$',
   f.name, f.sig, f.name, coalesce((select string_agg(split_part(trim(x), ' ', 1), ', ') from unnest(string_to_array(nullif(f.sig, ''), ',')) x), ''));
  execute format('revoke all on function public.geobr_%s(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function public.geobr_%s(%s) to authenticated', f.name, f.sig);
 end loop;
end $$;
revoke all on function geobr_private.natal_shop_offers(date) from public, anon, authenticated;

insert into geobr_private.releases(version, enabled, message)
values ('1.16.3', true, 'GeoBR 1.16.3: GeoBR Natal 2026 (teste)')
on conflict (version) do nothing;
