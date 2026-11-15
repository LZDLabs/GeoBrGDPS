-- GeoBR Beta 1.8: public cosmetic directory. Wallets and XP remain server-owned.
begin;
alter table public.geobr_profiles
 add column if not exists icon_id integer not null default 1 check(icon_id between 1 and 10000),
 add column if not exists color1 integer not null default 4382975 check(color1 between 0 and 16777215),
 add column if not exists color2 integer not null default 10573311 check(color2 between 0 and 16777215);
create index if not exists geobr_profiles_name_idx on public.geobr_profiles(lower(display_name),user_id);

create or replace function geobr_private.profile_sync(p_name text,p_icon integer,p_color1 integer,p_color2 integer)
returns jsonb language plpgsql security definer set search_path='' as $$
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 if p_name is null or char_length(trim(p_name)) not between 1 and 40 or p_name ~ '[[:cntrl:]]' then raise exception 'Nome invalido'; end if;
 if p_icon is null or p_icon not between 1 and 10000 or p_color1 is null or p_color1 not between 0 and 16777215 or p_color2 is null or p_color2 not between 0 and 16777215 then raise exception 'Icone invalido'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));
 perform geobr_private.ensure_v2();
 update public.geobr_profiles set display_name=trim(p_name),icon_id=p_icon,color1=p_color1,color2=p_color2,updated_at=now()
 where user_id=auth.uid() and (display_name,icon_id,color1,color2) is distinct from (trim(p_name),p_icon,p_color1,p_color2);
 return geobr_private.state_v2();
end $$;
create or replace function public.geobr_profile_sync(p_name text,p_icon integer,p_color1 integer,p_color2 integer)
returns jsonb language sql security invoker set search_path='' as $$
 select geobr_private.profile_sync(p_name,p_icon,p_color1,p_color2)
$$;
revoke all on function geobr_private.profile_sync(text,integer,integer,integer) from public,anon,authenticated;
grant execute on function geobr_private.profile_sync(text,integer,integer,integer) to authenticated;
revoke all on function public.geobr_profile_sync(text,integer,integer,integer) from public,anon;
grant execute on function public.geobr_profile_sync(text,integer,integer,integer) to authenticated;

create or replace function public.geobr_players(p_query text default '',p_offset integer default 0)
returns jsonb language plpgsql stable security invoker set search_path='' as $$
declare result jsonb;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 if p_offset is null or p_offset not between 0 and 300000 or char_length(coalesce(p_query,''))>40 then raise exception 'Busca invalida'; end if;
 with matched as materialized (
  select user_id,display_name,lifetime_xp,theme_slug,title_name,icon_id,color1,color2
  from public.geobr_profiles
  where strpos(lower(display_name),lower(trim(coalesce(p_query,''))))>0
 ), page as (
  select * from matched order by lower(display_name),user_id limit 3 offset p_offset
 ) select jsonb_build_object('players',coalesce((select jsonb_agg(to_jsonb(p) order by lower(p.display_name),p.user_id) from page p),'[]'::jsonb),'total',(select count(*) from matched)) into result;
 return result;
end $$;
revoke all on function public.geobr_players(text,integer) from public,anon;
grant execute on function public.geobr_players(text,integer) to authenticated;
comment on function public.geobr_players(text,integer) is 'Cosmetic profiles of GeoBR mod users; display names are not proof of GDPS account ownership. No wallet, auth token, or inventory is exposed.';
notify pgrst,'reload schema';
commit;
