-- Additive V1 candidate. Apply only to the original GeoBR Alpha backend.
create table public.geobr_level_ranks (
 id bigint generated always as identity primary key,
 level_id bigint not null unique check(level_id>0 and level_id<1500000000),
 position integer not null unique check(position>0),published boolean not null default true
);
alter table public.geobr_level_ranks enable row level security;
grant select on public.geobr_level_ranks to anon,authenticated;
grant insert,update,delete on public.geobr_level_ranks to authenticated;
grant usage on sequence public.geobr_level_ranks_id_seq to authenticated;
create policy ranks_read on public.geobr_level_ranks for select to anon,authenticated using(published);
create policy ranks_admin on public.geobr_level_ranks for all to authenticated using((select geobr_private.is_admin())) with check((select geobr_private.is_admin()));
-- Only the owner can create verified account links after checking GDPS ownership.
create table public.geobr_account_links (
 id bigint generated always as identity primary key,
 account_id bigint not null unique check(account_id>0),
 user_id uuid not null unique references auth.users(id) on delete cascade
);
alter table public.geobr_account_links enable row level security;
grant select,insert,update,delete on public.geobr_account_links to authenticated;
grant usage on sequence public.geobr_account_links_id_seq to authenticated;
create policy links_admin on public.geobr_account_links for all to authenticated using((select geobr_private.is_admin())) with check((select geobr_private.is_admin()));
create function public.geobr_native_profile(p_account bigint) returns jsonb
language sql stable security definer set search_path='' as $$
 select coalesce((select to_jsonb(p) - 'user_id' from public.geobr_profiles p join public.geobr_account_links l on l.user_id=p.user_id where l.account_id=p_account),'{}'::jsonb)
$$;
revoke all on function public.geobr_native_profile(bigint) from public,anon;
grant execute on function public.geobr_native_profile(bigint) to authenticated;
create table public.geobr_dm_messages (
 id uuid primary key,
 sender uuid not null references auth.users(id) on delete cascade,
 recipient uuid not null references auth.users(id) on delete cascade,
 body text not null check(length(btrim(body)) between 1 and 300),
 reply_to uuid references public.geobr_dm_messages(id),
 created_at timestamptz not null default now(),check(sender<>recipient)
);
create index geobr_dm_recipient on public.geobr_dm_messages(recipient,created_at desc);
create index geobr_dm_sender on public.geobr_dm_messages(sender,created_at desc);
alter table public.geobr_dm_messages enable row level security;
grant select on public.geobr_dm_messages to authenticated;
create policy dm_participants on public.geobr_dm_messages for select to authenticated using((select auth.uid()) in (sender,recipient));
create table public.geobr_dm_blocks (
 owner uuid not null references auth.users(id) on delete cascade,
 peer uuid not null references auth.users(id) on delete cascade,
 primary key(owner,peer),check(owner<>peer)
);
alter table public.geobr_dm_blocks enable row level security;
grant select,insert,delete on public.geobr_dm_blocks to authenticated;
create policy blocks_own on public.geobr_dm_blocks for all to authenticated using(owner=(select auth.uid())) with check(owner=(select auth.uid()));
create table public.geobr_social_notifications (
 id bigint generated always as identity primary key,
 owner uuid not null references auth.users(id) on delete cascade,
 actor uuid not null references auth.users(id) on delete cascade,
 kind text not null check(kind in ('dm','reply')),
 body text not null,created_at timestamptz not null default now()
);
alter table public.geobr_social_notifications enable row level security;
grant select on public.geobr_social_notifications to authenticated;
create policy social_own on public.geobr_social_notifications for select to authenticated using(owner=(select auth.uid()));
create index social_owner_id on public.geobr_social_notifications(owner,id);
create function public.geobr_dm_send(p_peer uuid,p_body text,p_id uuid,p_reply uuid default null) returns jsonb
language plpgsql security definer set search_path='' as $$
declare u uuid=auth.uid(); old public.geobr_dm_messages; n text;
begin
 if u is null or p_peer is null or p_peer=u or p_id is null then raise exception 'Destino invalido';end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-social:'||u::text,0));
 select * into old from public.geobr_dm_messages where id=p_id;
 if found then
  if old.sender=u and old.recipient=p_peer and old.body=p_body and old.reply_to is not distinct from p_reply then return jsonb_build_object('ok',true);end if;
  raise exception 'Identidade de mensagem invalida';
 end if;
 if not exists(select 1 from public.geobr_profiles where user_id=p_peer) then raise exception 'Jogador nao encontrado';end if;
 if exists(select 1 from public.geobr_dm_blocks where (owner=u and peer=p_peer) or (owner=p_peer and peer=u)) then raise exception 'Conversa bloqueada';end if;
 if exists(select 1 from public.geobr_dm_messages where sender=u and created_at>now()-interval '3 seconds') then raise exception 'Aguarde 3 segundos';end if;
 if (select count(*) from public.geobr_dm_messages where sender=u and created_at>now()-interval '1 hour')>=100 then raise exception 'Limite de mensagens atingido';end if;
 if p_body is null or length(btrim(p_body)) not between 1 and 300 then raise exception 'Mensagem invalida';end if;
 if p_reply is not null and not exists(select 1 from public.geobr_dm_messages where id=p_reply and ((sender=u and recipient=p_peer) or (sender=p_peer and recipient=u))) then raise exception 'Resposta fora da conversa';end if;
 insert into public.geobr_dm_messages(id,sender,recipient,body,reply_to) values(p_id,u,p_peer,p_body,p_reply);
 select display_name into n from public.geobr_profiles where user_id=u;
 insert into public.geobr_social_notifications(owner,actor,kind,body) values(p_peer,u,'dm',coalesce(n,'Jogador')||': '||left(p_body,100));
 return jsonb_build_object('ok',true);
end $$;
create function public.geobr_dm_thread(p_peer uuid) returns jsonb
language sql stable security invoker set search_path='' as $$
 select coalesce(jsonb_agg(to_jsonb(t) order by t.created_at desc),'[]'::jsonb) from (
 select m.id,m.body,m.reply_to,m.created_at,m.sender,p.display_name as sender_name
 from public.geobr_dm_messages m join public.geobr_profiles p on p.user_id=m.sender
 where (m.sender=auth.uid() and m.recipient=p_peer) or (m.sender=p_peer and m.recipient=auth.uid())
 order by m.created_at desc,m.id limit 50) t
$$;
create function public.geobr_dm_inbox() returns jsonb
language sql stable security invoker set search_path='' as $$
 select coalesce(jsonb_agg(to_jsonb(t) order by t.created_at desc),'[]'::jsonb) from (
 select distinct on (case when m.sender=auth.uid() then m.recipient else m.sender end)
 case when m.sender=auth.uid() then m.recipient else m.sender end as peer,p.display_name as name,m.body,m.created_at
 from public.geobr_dm_messages m join public.geobr_profiles p on p.user_id=case when m.sender=auth.uid() then m.recipient else m.sender end
 where auth.uid() in (m.sender,m.recipient)
 order by case when m.sender=auth.uid() then m.recipient else m.sender end,m.created_at desc,m.id
 limit 100) t
$$;
create function public.geobr_dm_block(p_peer uuid) returns jsonb
language plpgsql security invoker set search_path='' as $$
begin
 insert into public.geobr_dm_blocks(owner,peer) values(auth.uid(),p_peer) on conflict do nothing;
 return jsonb_build_object('ok',true);
end $$;
alter table public.geobr_community_messages add column reply_to bigint references public.geobr_community_messages(id);
create function geobr_private.notify_chat_reply() returns trigger language plpgsql security definer set search_path='' as $$
declare target uuid;
begin
 if new.reply_to is not null then
 select user_id into target from public.geobr_community_messages where id=new.reply_to;
 if target is not null and target<>new.user_id then
 insert into public.geobr_social_notifications(owner,actor,kind,body) values(target,new.user_id,'reply',new.username||' respondeu: '||left(new.message,100));
 end if;end if;return new;
end $$;
revoke all on function geobr_private.notify_chat_reply() from public,anon,authenticated;
create trigger geobr_chat_reply after insert on public.geobr_community_messages for each row execute function geobr_private.notify_chat_reply();
revoke all on function public.geobr_dm_send(uuid,text,uuid,uuid),public.geobr_dm_thread(uuid),public.geobr_dm_inbox(),public.geobr_dm_block(uuid) from public,anon;
grant execute on function public.geobr_dm_send(uuid,text,uuid,uuid),public.geobr_dm_thread(uuid),public.geobr_dm_inbox(),public.geobr_dm_block(uuid) to authenticated;
-- Twelve additional real quest definitions, preserving existing assigned quest snapshots.
insert into public.geobr_quest_steps(season_id,sequence_order,title,kind,goal,difficulty,scope,mode,platformer,min_percent,max_percent,xp,tokens)
select s.id,b.base+v.n,v.title,v.kind,v.goal,v.diff,v.scope,v.mode,v.plat,v.lo,v.hi,v.xp,v.tokens
from public.geobr_pass_seasons s cross join lateral(select coalesce(max(sequence_order),0) as base from public.geobr_quest_steps where season_id=s.id) b
cross join (values
 (1,'Explore 3 Main Levels','reach',3,'any','main','normal','classic',50,100,150,10),
 (2,'Complete 2 niveis Hard','complete',2,'hard','online','normal','classic',100,100,200,12),
 (3,'Pratique um Demon','complete',1,'demon','online','practice','classic',100,100,125,8),
 (4,'Complete 3 plataformas','complete',3,'any','online','normal','platformer',100,100,200,12),
 (5,'Chegue a 80% em 2 niveis','reach',2,'any','online','normal','classic',80,100,150,10),
 (6,'Complete 3 oficiais','complete',3,'any','official','normal','classic',100,100,175,10),
 (7,'Complete 2 Main Levels','complete',2,'any','main','normal','any',100,100,200,12),
 (8,'Complete 2 Insanes','complete',2,'insane','online','normal','classic',100,100,250,15),
 (9,'Chegue a 60% em um Demon','reach',1,'demon','online','normal','classic',60,100,200,12),
 (10,'Complete 4 Easy diferentes','complete',4,'easy','online','normal','any',100,100,150,10),
 (11,'Complete um Demon','complete',1,'demon','online','normal','classic',100,100,300,20),
 (12,'Complete 5 niveis diferentes','complete',5,'any','online','normal','any',100,100,250,15)
) v(n,title,kind,goal,diff,scope,mode,plat,lo,hi,xp,tokens);
