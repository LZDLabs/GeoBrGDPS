create table geobr_private.dm_reads(owner uuid not null references auth.users(id),peer uuid not null references auth.users(id),seen_at timestamptz not null default now(),primary key(owner,peer));
alter table geobr_private.dm_reads enable row level security;
revoke all on geobr_private.dm_reads from public,anon,authenticated;
create or replace function geobr_private.dm_thread_v5(p_peer uuid) returns jsonb language plpgsql security definer set search_path='' as $$
declare u uuid=auth.uid();r jsonb;begin
 perform geobr_private.require_access();
 select coalesce(jsonb_agg(to_jsonb(t) order by t.created_at desc,t.id),'[]') into r from (
 select m.id,m.sender,m.body,m.reply_to,m.created_at,p.display_name sender_name,p.icon_id,p.icon_slug,p.color1,p.color2,
 (select left(reply.body,80) from public.geobr_dm_messages reply where reply.id=m.reply_to and u in(reply.sender,reply.recipient)) reply_body
 from public.geobr_dm_messages m join public.geobr_profiles p on p.user_id=m.sender
 where (m.sender=u and m.recipient=p_peer) or (m.sender=p_peer and m.recipient=u) order by m.created_at desc,m.id limit 50)t;
 insert into geobr_private.dm_reads(owner,peer,seen_at) values(u,p_peer,now()) on conflict(owner,peer) do update set seen_at=excluded.seen_at;
 return r;
end $$;
revoke all on function geobr_private.dm_thread_v5(uuid) from public,anon;
grant execute on function geobr_private.dm_thread_v5(uuid) to authenticated;
create or replace function public.geobr_dm_thread(p_peer uuid) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.dm_thread_v5(p_peer)$$;
create or replace function geobr_private.dm_inbox_v5() returns jsonb language plpgsql security definer set search_path='' as $$
declare u uuid=auth.uid();r jsonb;begin perform geobr_private.require_access();
 select coalesce(jsonb_agg(to_jsonb(t) order by t.created_at desc),'[]') into r from (
 select distinct on(case when m.sender=u then m.recipient else m.sender end)
 case when m.sender=u then m.recipient else m.sender end peer,p.display_name name,m.body,m.created_at,p.icon_id,p.icon_slug,p.color1,p.color2,
 (select count(*) from public.geobr_dm_messages unread where unread.sender=p.user_id and unread.recipient=u and unread.created_at>coalesce((select seen_at from geobr_private.dm_reads where owner=u and peer=p.user_id),'-infinity')) unread
 from public.geobr_dm_messages m join public.geobr_profiles p on p.user_id=case when m.sender=u then m.recipient else m.sender end
 where u in(m.sender,m.recipient) order by case when m.sender=u then m.recipient else m.sender end,m.created_at desc,m.id limit 100)t;return r;
end $$;
revoke all on function geobr_private.dm_inbox_v5() from public,anon;
grant execute on function geobr_private.dm_inbox_v5() to authenticated;
create or replace function public.geobr_dm_inbox() returns jsonb language sql security invoker set search_path='' as $$select geobr_private.dm_inbox_v5()$$;
create or replace function public.geobr_chat_feed() returns jsonb language sql stable security invoker set search_path='' as $$
select coalesce(jsonb_agg(to_jsonb(t) order by created_at desc,id desc),'[]') from (
 select m.*,p.icon_id,p.icon_slug,p.color1,p.color2,
 (select left(reply.message,70) from public.geobr_community_messages reply where reply.id=m.reply_to) reply_body
 from public.geobr_community_messages m left join public.geobr_profiles p on p.user_id=m.user_id order by m.created_at desc,m.id desc limit 50)t
$$;
grant execute on function public.geobr_chat_feed() to anon,authenticated;
create or replace function geobr_private.read_allowed() returns boolean language sql stable security definer set search_path='' as $$select coalesce((geobr_private.access_status()->>'allowed')::boolean,false)$$;
revoke all on function geobr_private.read_allowed() from public,anon;
grant execute on function geobr_private.read_allowed() to authenticated;
create policy dm_mod_access on public.geobr_dm_messages as restrictive for select to authenticated using(geobr_private.read_allowed());
