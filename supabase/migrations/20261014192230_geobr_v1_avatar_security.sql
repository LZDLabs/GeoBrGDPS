alter table public.geobr_profiles add column icon_slug text not null default '' check(length(icon_slug)<=120);
create function geobr_private.profile_sync_v2(p_name text,p_icon integer,p_color1 integer,p_color2 integer,p_icon_slug text default '') returns jsonb
language plpgsql security definer set search_path='' as $$
begin
 if auth.uid() is null then raise exception 'Sessao necessaria';end if;
 if p_icon_slug is null or length(p_icon_slug)>120 or p_icon_slug ~ '[[:cntrl:]]' then raise exception 'Icone invalido';end if;
 perform geobr_private.profile_sync(p_name,p_icon,p_color1,p_color2);
 update public.geobr_profiles set icon_slug=p_icon_slug,updated_at=now() where user_id=auth.uid() and icon_slug is distinct from p_icon_slug;
 return geobr_private.state_v2();
end $$;
create function public.geobr_profile_sync_v2(p_name text,p_icon integer,p_color1 integer,p_color2 integer,p_icon_slug text default '') returns jsonb
language sql security invoker set search_path='' as $$select geobr_private.profile_sync_v2(p_name,p_icon,p_color1,p_color2,p_icon_slug)$$;
revoke all on function geobr_private.profile_sync_v2(text,integer,integer,integer,text),public.geobr_profile_sync_v2(text,integer,integer,integer,text) from public,anon;
grant execute on function geobr_private.profile_sync_v2(text,integer,integer,integer,text),public.geobr_profile_sync_v2(text,integer,integer,integer,text) to authenticated;
create or replace function public.geobr_players(p_query text default '',p_offset integer default 0) returns jsonb
language plpgsql stable security invoker set search_path='' as $$
declare result jsonb;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria';end if;
 if p_offset is null or p_offset not between 0 and 300000 or char_length(coalesce(p_query,''))>40 then raise exception 'Busca invalida';end if;
 with matched as materialized (
 select user_id,display_name,lifetime_xp,theme_slug,title_name,icon_id,color1,color2,icon_slug from public.geobr_profiles
 where strpos(lower(display_name),lower(trim(coalesce(p_query,''))))>0
 ),page as (select * from matched order by lower(display_name),user_id limit 3 offset p_offset)
 select jsonb_build_object('players',coalesce((select jsonb_agg(to_jsonb(p) order by lower(p.display_name),p.user_id) from page p),'[]'::jsonb),'total',(select count(*) from matched)) into result;
 return result;
end $$;
-- Keep privileged implementation outside the exposed API schema.
alter function public.geobr_dm_send(uuid,text,uuid,uuid) set schema geobr_private;
create function public.geobr_dm_send(p_peer uuid,p_body text,p_id uuid,p_reply uuid default null) returns jsonb
language sql security invoker set search_path='' as $$select geobr_private.geobr_dm_send(p_peer,p_body,p_id,p_reply)$$;
alter function public.geobr_native_profile(bigint) set schema geobr_private;
create function public.geobr_native_profile(p_account bigint) returns jsonb
language sql stable security invoker set search_path='' as $$select geobr_private.geobr_native_profile(p_account)$$;
revoke all on function public.geobr_dm_send(uuid,text,uuid,uuid),public.geobr_native_profile(bigint) from public,anon;
grant execute on function public.geobr_dm_send(uuid,text,uuid,uuid),public.geobr_native_profile(bigint) to authenticated;
notify pgrst,'reload schema';
