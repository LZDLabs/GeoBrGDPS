create index if not exists geobr_collab_rooms_owner_idx
    on public.geobr_collab_rooms (owner_id);
create index if not exists geobr_collab_operations_author_idx
    on public.geobr_collab_operations (author_id);
create index if not exists geobr_collab_snapshots_updated_by_idx
    on public.geobr_collab_snapshots (updated_by)
    where updated_by is not null;

drop policy if exists "GeoBR published levels are public" on public.geobr_main_levels;
