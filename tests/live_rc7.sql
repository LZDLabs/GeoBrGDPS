-- GeoBR RC7 live checks for Arrow items. ROLLS BACK: nothing persists.
begin;
insert into auth.users(id,instance_id,aud,role,email,created_at,updated_at,is_anonymous) values
('9cf657ba-740a-4903-9c85-000000000071','00000000-0000-0000-0000-000000000000','authenticated','authenticated','geobr-rc7-a@example.invalid',now(),now(),true);
select set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000071',true);
set local role authenticated;
select public.geobr_state_v2();
reset role;
update public.geobr_wallets set tokens=500 where user_id='9cf657ba-740a-4903-9c85-000000000071';
set local role authenticated;
do $$declare s jsonb;denied boolean;item jsonb;begin
 -- Free skins equip, locked ones are refused.
 perform public.geobr_profile_arrow(3,false);
 denied=false;begin perform public.geobr_profile_arrow(5,false);exception when others then denied=true;end;assert denied,'shop skin needs purchase';
 -- Buying an arrow: charged once, owned, equippable, flagged as equipped.
 s=public.geobr_buy_item('arrow-5',gen_random_uuid(),80,null,null);
 assert (s->>'tokens')::int=420,'arrow-5 charged 80';
 s=public.geobr_buy_item('arrow-5',gen_random_uuid(),80,null,null);
 assert (s->>'tokens')::int=420,'owned arrow is not charged twice';
 perform public.geobr_profile_arrow(5,true);
 s=public.geobr_state_v2();
 select x into item from jsonb_array_elements(s->'shop') x where x->>'id'='arrow-5';
 assert (item->>'owned')::boolean and (item->>'equipped')::boolean,'arrow-5 owned and equipped in state';
 -- Pass and crate exclusives cannot be bought.
 denied=false;begin perform public.geobr_buy_item('arrow-9',gen_random_uuid(),0,null,null);exception when others then denied=true;end;assert denied,'pass arrow not for sale';
 denied=false;begin perform public.geobr_buy_item('arrow-11',gen_random_uuid(),0,null,null);exception when others then denied=true;end;assert denied,'crate arrow not for sale';
 denied=false;begin perform public.geobr_profile_arrow(16,false);exception when others then denied=true;end;assert denied,'crate arrow needs the item';
 denied=false;begin perform public.geobr_profile_arrow(17,false);exception when others then denied=true;end;assert denied,'skin outside catalog';
end $$;
reset role;
-- Crate/pass delivery path: first copy is the item, the duplicate becomes Tokens.
do $$declare r jsonb;begin
 perform set_config('request.jwt.claim.sub','9cf657ba-740a-4903-9c85-000000000071',true);
 r=geobr_private.deliver('rc7-test:1',geobr_private.crate_arrow(6));
 assert r->>'kind'='arrow' and r->>'item_id'='arrow-16' and (r->>'exclusive')::boolean,'mythic crate arrow delivered';
 r=geobr_private.deliver('rc7-test:2',geobr_private.crate_arrow(6));
 assert r->>'kind'='tokens' and (r->>'duplicate')::boolean,'duplicate arrow converted';
 assert exists(select 1 from public.geobr_inventory where user_id=auth.uid() and item_id='arrow-16'),'arrow-16 in inventory';
 assert geobr_private.crate_arrow(1)->>'item_id'='arrow-11' and geobr_private.crate_arrow(4)->>'item_id'='arrow-14','one exclusive per rarity';
 assert position('crate_arrow' in pg_get_functiondef('geobr_private.open_crate_v4(uuid)'::regprocedure))>0,'crates roll the exclusive arrow';
end $$;
set local role authenticated;
do $$begin perform public.geobr_profile_arrow(16,true);end $$;
reset role;
do $$begin
 assert (select count(*) from public.geobr_pass_rewards r join public.geobr_pass_seasons s on s.id=r.season_id
         where s.published and s.starts_at<=now() and s.ends_at>now() and r.kind='arrow' and r.tier in (31,32))=2,'active season has the two bonus arrow tiers';
 assert (select count(*) from public.geobr_shop_items where kind='arrow')=12,'12 arrow items';
 assert exists(select 1 from geobr_private.releases where version='1.9.0-beta.7' and enabled),'RC7 release enabled';
 assert not has_function_privilege('authenticated','geobr_private.crate_arrow(integer)','execute'),'crate_arrow is internal';
end $$;
select 'PASS live_rc7: arrow purchase/equip/ownership, pass+crate exclusivity, delivery + duplicates, bonus tiers, release row' as result;
rollback;
