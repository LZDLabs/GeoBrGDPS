-- Public downloads of the released mods (links for videos). Read-only for everyone;
-- the upload permission below only accepted the two files and expired by itself.
insert into storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
values ('geobr-downloads', 'geobr-downloads', true, 20971520, array['application/octet-stream'])
on conflict (id) do nothing;

create policy geobr_tmp_download_upload on storage.objects for insert to anon
with check (bucket_id = 'geobr-downloads'
  and name in ('geobr.geobrmod.geode', 'geobr.gdps.geode')
  and now() < '2026-11-20 21:00:00+00');
