-- GeoBR 1.13.0 (final): chat with each sender's current skin (glow included),
-- the GeoBR admin panel opened from the GDPS Owner panel (the Owner's GDPS
-- account replaces the panel password), only the first hosted Main Level
-- published. Non-destructive: function replacements, an UPDATE and a release row.

create or replace function public.geobr_chat_feed()
returns jsonb language sql stable set search_path = '' as $function$
select coalesce(jsonb_agg(to_jsonb(t) order by created_at desc, id desc), '[]') from (
 select m.*, p.icon_id, p.icon_slug, p.color1, p.color2, p.glow, p.glow_color, p.icons,
 (select left(reply.message, 70) from public.geobr_community_messages reply where reply.id = m.reply_to) reply_body
 from public.geobr_community_messages m left join public.geobr_profiles p on p.user_id = m.user_id
 order by m.created_at desc, m.id desc limit 50) t
$function$;

-- GDPS Owner -> GeoBR admin session (20 minutes) for this device, checked
-- against the Owner's GDPS account (accountID + gjp2, bcrypt) inside the DB.
create or replace function geobr_private.admin_login_gdps(p_account bigint, p_gjp2 text)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare a geobr_private.admin_attempts; ok boolean;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 insert into geobr_private.admin_attempts(user_id) values (auth.uid()) on conflict do nothing;
 select * into a from geobr_private.admin_attempts where user_id = auth.uid() for update;
 if a.window_start < now() - interval '15 minutes' then
  update geobr_private.admin_attempts set failures = 0, window_start = now() where user_id = auth.uid();
  a.failures = 0;
 end if;
 if a.failures >= 5 then return jsonb_build_object('ok', false, 'error', 'Aguarde 15 minutos antes de tentar novamente.'); end if;
 select coalesce(p_gjp2 ~ '^[0-9a-f]{40}$' and extensions.crypt(p_gjp2, acc.gjp2_hash) = acc.gjp2_hash
   and exists (select 1 from geobr_gdps.account_roles ar join geobr_gdps.roles r using (role_id) where ar.account_id = acc.account_id and r.is_owner)
   and not acc.is_banned, false)
  into ok from geobr_gdps.accounts acc where acc.account_id = p_account;
 if not coalesce(ok, false) then
  update geobr_private.admin_attempts set failures = failures + 1 where user_id = auth.uid();
  return jsonb_build_object('ok', false, 'error', 'Somente o Owner do GDPS abre este painel.');
 end if;
 insert into geobr_private.admin_sessions(user_id, expires_at) values (auth.uid(), now() + interval '20 minutes')
  on conflict (user_id) do update set expires_at = excluded.expires_at;
 update geobr_private.admin_attempts set failures = 0 where user_id = auth.uid();
 insert into geobr_gdps.audit_log (actor, action, target_type, target_id, details)
  values (p_account, 'owner.geobr_panel', 'account', p_account::text, '{}'::jsonb);
 return jsonb_build_object('ok', true, 'expires_at', now() + interval '20 minutes');
end $$;
revoke all on function geobr_private.admin_login_gdps(bigint, text) from public, anon;
grant execute on function geobr_private.admin_login_gdps(bigint, text) to authenticated;
create or replace function public.geobr_admin_login_gdps(p_account bigint, p_gjp2 text) returns jsonb
language sql security invoker set search_path = '' as $$ select geobr_private.admin_login_gdps(p_account, p_gjp2) $$;
revoke all on function public.geobr_admin_login_gdps(bigint, text) from public, anon;
grant execute on function public.geobr_admin_login_gdps(bigint, text) to authenticated;

-- Only the first hosted Main Level stays published (the others are kept, unpublished).
update public.geobr_main_levels set published = false where campaign_order > 1 and published;

insert into geobr_private.releases(version, enabled, message)
values ('1.13.0', true, 'GeoBR 1.13.0')
on conflict (version) do nothing;
