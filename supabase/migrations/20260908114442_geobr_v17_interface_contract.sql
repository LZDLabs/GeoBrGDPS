create or replace function geobr_private.state_v2() returns jsonb language plpgsql security definer set search_path='' as $$
declare base jsonb; profile public.geobr_profiles; sid bigint; lv bigint; quest jsonb;
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-economy:'||auth.uid()::text,0));
 perform geobr_private.ensure_v2();
 base=geobr_private.pass_state()-'quests'; sid=(base->'season'->>'id')::bigint;
 select * into profile from public.geobr_profiles where user_id=auth.uid();
 lv=floor(sqrt(profile.lifetime_xp::numeric/100))::bigint+1;
 select definition||jsonb_build_object('run_id',run_id,'progress',progress,'completed_total',completed_total) into quest
 from geobr_private.quest_runs where user_id=auth.uid() and season_id=sid;
 return base||jsonb_build_object(
  'revision',floor(extract(epoch from clock_timestamp())*1000000)::bigint,
  'profile',to_jsonb(profile)||jsonb_build_object('level',lv,'level_start_xp',100*(lv-1)*(lv-1),'next_level_xp',100*lv*lv),
  'quest',quest,'quest_count',(select count(*) from public.geobr_quest_steps where season_id=sid and published),
  'shop',coalesce((select jsonb_agg(to_jsonb(i)||jsonb_build_object(
   'owned',exists(select 1 from public.geobr_inventory v where v.user_id=auth.uid() and v.item_id=i.id),
   'equipped',case when kind='theme' then i.style=profile.theme_slug when kind='title' then i.name=profile.title_name else false end)
   order by sort_order,id) from public.geobr_shop_items i where published),'[]'::jsonb));
end $$;

create view public.geobr_bulletins with (security_invoker=true) as
 select 'news:'||id::text as id,title,description,author,type,pinned,priority,published,created_at from public.geobr_news where published
 union all
 select 'notice:'||id::text,title,message,'Equipe GeoBR','notice',true,100::smallint,published,created_at
 from public.geobr_announcements where published and expires_at>now();
grant select on public.geobr_bulletins to anon,authenticated;
