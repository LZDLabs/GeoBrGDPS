-- GeoBR 1.15.0 (part 2): social missions and notifications straight from the
-- GDPS tables (any client, no server code change), and online status for the
-- mod through Supabase. Non-destructive: new functions and triggers only.

-- Comments, likes, difficulty votes, friendships, friend requests and GD
-- messages advance the matching mission; requests and messages also notify.
-- Errors here never block the player's action.
create or replace function geobr_private.gdps_social_trigger()
returns trigger language plpgsql security definer set search_path = '' as $$
declare u uuid; other uuid; name text;
begin
 begin
  if tg_table_name = 'comments' then
   select supabase_user into u from geobr_gdps.accounts where account_id = new.account_id;
   perform geobr_private.social_progress(u, 'comment', new.level_id::text, 1);
  elsif tg_table_name = 'likes' then
   if new.item_type = 1 and new.value > 0 and new.voter like 'a:%' then
    select supabase_user into u from geobr_gdps.accounts where account_id = substr(new.voter, 3)::bigint;
    perform geobr_private.social_progress(u, 'like', new.item_id::text, 1);
   end if;
  elsif tg_table_name = 'rate_votes' then
   select supabase_user into u from geobr_gdps.accounts where account_id = new.account_id;
   perform geobr_private.social_progress(u, 'rate', new.level_id::text, 1);
  elsif tg_table_name = 'friendships' then
   select supabase_user into u from geobr_gdps.accounts where account_id = new.a;
   select supabase_user into other from geobr_gdps.accounts where account_id = new.b;
   perform geobr_private.social_progress(u, 'friend', new.b::text, 1);
   perform geobr_private.social_progress(other, 'friend', new.a::text, 1);
  elsif tg_table_name = 'friend_requests' then
   select supabase_user, username into u, name from geobr_gdps.accounts where account_id = new.from_account;
   perform geobr_private.notify_account(new.to_account, u, 'friend', coalesce(name, 'Alguem') || ' te enviou um pedido de amizade');
  elsif tg_table_name = 'messages' then
   select supabase_user, username into u, name from geobr_gdps.accounts where account_id = new.from_account;
   perform geobr_private.social_progress(u, 'gd_message', new.to_account::text, 1);
   perform geobr_private.notify_account(new.to_account, u, 'gd_message', coalesce(name, 'Alguem') || ' te mandou uma mensagem');
  end if;
 exception when others then
  raise warning 'gdps_social_trigger %: %', tg_table_name, sqlerrm;
 end;
 return null;
end $$;
revoke all on function geobr_private.gdps_social_trigger() from public, anon, authenticated;

do $$
declare t text;
begin
 foreach t in array array['comments', 'likes', 'friendships', 'friend_requests', 'messages'] loop
  execute format('create or replace trigger geobr_social after insert on geobr_gdps.%I for each row execute function geobr_private.gdps_social_trigger()', t);
 end loop;
 create or replace trigger geobr_social after insert or update on geobr_gdps.rate_votes for each row execute function geobr_private.gdps_social_trigger();
end $$;

-- Online status: the mod pings every 2 minutes (GDPS accounts linked to this
-- GeoBR player) and reads the status of the profiles it shows.
create or replace function geobr_private.presence_ping(p_visibility text)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare n int;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 update geobr_gdps.accounts set last_seen = now(),
  presence_visibility = case when p_visibility in ('all', 'friends', 'none') then p_visibility else presence_visibility end
  where supabase_user = auth.uid();
 get diagnostics n = row_count;
 return jsonb_build_object('ok', true, 'accounts', n);
end $$;

create or replace function geobr_private.presence_get(p_accounts bigint[])
returns jsonb language sql stable security definer set search_path = '' as $$
 with me as (select account_id from geobr_gdps.accounts where supabase_user = auth.uid()),
 t as (
  select a.account_id::int as id, a.last_seen, a.presence_visibility as visibility,
   exists (select 1 from me where me.account_id = a.account_id) as self,
   exists (select 1 from geobr_gdps.friendships f join me on true
    where (f.a = a.account_id and f.b = me.account_id) or (f.b = a.account_id and f.a = me.account_id)) as friend
  from geobr_gdps.accounts a where a.account_id = any(coalesce(p_accounts, '{}')) limit 50)
 select coalesce(jsonb_agg(case
   when not t.self and (t.visibility = 'none' or (t.visibility = 'friends' and not t.friend))
    then jsonb_build_object('account_id', t.id, 'status', 'hidden')
   else jsonb_build_object('account_id', t.id, 'last_seen', t.last_seen,
    'status', case when t.last_seen > now() - interval '5 minutes' then 'online'
                   when t.last_seen > now() - interval '3 days' then 'idle' else 'away' end,
    'visibility', case when t.self then t.visibility end)
  end), '[]'::jsonb) from t
$$;

do $$
declare f record;
begin
 for f in select * from (values ('presence_ping', 'p_visibility text'), ('presence_get', 'p_accounts bigint[]')) as t(name, sig) loop
  execute format('revoke all on function geobr_private.%I(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function geobr_private.%I(%s) to authenticated', f.name, f.sig);
  execute format('create or replace function public.geobr_%s(%s) returns jsonb language sql set search_path = '''' as $f$ select geobr_private.%I(%s) $f$',
   f.name, f.sig, f.name, split_part(f.sig, ' ', 1));
  execute format('revoke all on function public.geobr_%s(%s) from public, anon', f.name, f.sig);
  execute format('grant execute on function public.geobr_%s(%s) to authenticated', f.name, f.sig);
 end loop;
end $$;
