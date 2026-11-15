create function geobr_private.save_rank_v3(p_id bigint,p_level bigint,p_position integer,p_published boolean) returns jsonb language plpgsql security definer set search_path='' as $$
declare r public.geobr_level_ranks;
begin
 if auth.uid() is null or not geobr_private.is_admin() then raise exception 'Sessao ADM expirada. Entre novamente.';end if;
 if p_id is null then return geobr_private.save_rank(p_level,p_position,p_published);end if;
 if p_level is null or p_level<=0 or p_level>=1500000000 or p_position is null or p_position<1 then raise exception 'ID e TOP devem ser maiores que zero';end if;
 perform pg_advisory_xact_lock(hashtextextended('geobr-ranks',0));
 if exists(select 1 from public.geobr_level_ranks where id<>p_id and (position=p_position or level_id=p_level)) then raise exception 'ID ou posicao ja usados em outro registro';end if;
 update public.geobr_level_ranks set level_id=p_level,position=p_position,published=p_published where id=p_id returning * into r;
 if not found then raise exception 'Registro removido. Atualize a lista.';end if;
 return to_jsonb(r);
end $$;
create function public.geobr_save_rank_v3(p_id bigint,p_level bigint,p_position integer,p_published boolean) returns jsonb language sql security invoker set search_path='' as $$select geobr_private.save_rank_v3(p_id,p_level,p_position,p_published)$$;
revoke all on function geobr_private.save_rank_v3(bigint,bigint,integer,boolean),public.geobr_save_rank_v3(bigint,bigint,integer,boolean) from public,anon;
grant execute on function geobr_private.save_rank_v3(bigint,bigint,integer,boolean),public.geobr_save_rank_v3(bigint,bigint,integer,boolean) to authenticated;
notify pgrst,'reload schema';
