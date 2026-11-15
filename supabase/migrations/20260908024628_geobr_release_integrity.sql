-- Retire Collab entry points without deleting historical level data.
revoke all on table public.geobr_collab_rooms,public.geobr_collab_members,
 public.geobr_collab_operations,public.geobr_collab_snapshots from anon,authenticated;
revoke execute on function public.geobr_create_collab_room(text,text),
 public.geobr_join_collab_room(text,text),public.geobr_save_collab_snapshot(uuid,text,bigint)
 from public,anon,authenticated;
alter table public.geobr_community_messages alter column user_id set default auth.uid();

-- The old manually-created catalog lacked the validation used by the client.
alter table public.geobr_main_levels add constraint geobr_main_metadata check(
 id>0 and id<400000000 and campaign_order>0
 and length(name) between 1 and 64 and length(creator) between 1 and 40
 and length(description)<=300 and stars between 0 and 10
 and file_version between 1 and 1000000
 and lower(difficulty) in ('auto','easy','normal','hard','harder','insane','demon',
 'easy demon','medium demon','hard demon','insane demon','extreme demon')
 and length(gmd_path) between 5 and 240
 and left(gmd_path,1)<>'/' and strpos(gmd_path,chr(92))=0
 and strpos(gmd_path,'..')=0 and strpos(gmd_path,'://')=0
 and lower(gmd_path) ~ '\.(gmd|gmd2|lvl)$'
);
-- Published Main Levels must reference a nonempty file actually present in Storage.
create function geobr_private.validate_main_file() returns trigger
language plpgsql security definer set search_path='' as $$
begin
 if new.published and not exists(select 1 from storage.objects
 where bucket_id='geobr-main-levels' and name=new.gmd_path
 and coalesce((metadata->>'size')::bigint,0)>0) then
  raise exception 'Envie um arquivo .gmd nao vazio antes de publicar o Main Level.';
 end if;
 new.updated_at=now();return new;
end $$;
revoke all on function geobr_private.validate_main_file() from public,anon,authenticated;
create trigger geobr_main_validate_file before insert or update on public.geobr_main_levels
 for each row execute function geobr_private.validate_main_file();

-- Clearly bounded native skin IDs for GD 2.2081, matching GameManager::countForType at runtime.
alter table public.geobr_pass_rewards add constraint geobr_reward_limits check (
 (kind<>'stars' or amount<=100) and (kind<>'skin' or amount=1)
);
-- Community posts use the auth subject, with server-side size/cooldown enforcement.
create function geobr_private.validate_message() returns trigger
language plpgsql security definer set search_path='' as $$
begin
 if auth.uid() is null or new.user_id is distinct from auth.uid() then raise exception 'Sessao invalida'; end if;
 if length(btrim(new.message))<1 or length(new.message)>160 or length(new.username)>40 then raise exception 'Mensagem invalida'; end if;
 perform pg_advisory_xact_lock(hashtextextended(auth.uid()::text,0));
 if exists(select 1 from public.geobr_community_messages where user_id=auth.uid() and created_at>now()-interval '3 seconds') then raise exception 'Aguarde 3 segundos antes de enviar novamente'; end if;
 new.created_at=now();return new;
end $$;
revoke all on function geobr_private.validate_message() from public,anon,authenticated;
create trigger geobr_community_validate before insert on public.geobr_community_messages
 for each row execute function geobr_private.validate_message();
create index if not exists geobr_community_user_created on public.geobr_community_messages(user_id,created_at desc);
;

