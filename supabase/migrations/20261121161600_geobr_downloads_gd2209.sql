-- GD 2.2091 (Geode 6) downloads of GeoBR 1.15.3 and GeoBR GDPS 1.0.3 (no Natal
-- content), in their own folder so the GD 2.2081 links stay as they are.
-- Insert only (nothing can be replaced), two exact names, expires by itself.
create policy geobr_tmp_download_upload_gd2209 on storage.objects for insert to anon
with check (bucket_id = 'geobr-downloads'
  and name in ('gd2209/geobr.geobrmod.geode', 'gd2209/geobr.gdps.geode')
  and now() < '2026-11-21 17:15:00+00');
