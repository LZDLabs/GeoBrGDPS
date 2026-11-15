-- GeoBR RC8 (1.9.0-beta.8) — original campaign levels hosted in Storage. Forward-only.
--
-- The two .gmd files (campaign/geobr-*.gmd, designed by tools/design_campaign.py)
-- were uploaded to the public bucket geobr-main-levels as
-- main-levels/geobr-{teoria-tropical,bloqueio-total}-v1.gmd through temporary
-- policies limited to exact object names (the earlier RobTop-based files were
-- deleted the same way). This migration removes those policies, lists the
-- levels in the campaign and enables RC8.

-- 1) Temporary upload/cleanup policies (anon, exact object names only).
drop policy if exists geobr_tmp_seed_rc8 on storage.objects;
drop policy if exists geobr_tmp_seed_rc8b_insert on storage.objects;
drop policy if exists geobr_tmp_seed_rc8b_select on storage.objects;
drop policy if exists geobr_tmp_seed_rc8b_delete on storage.objects;

-- 2) Campaign rows (after the existing level 1).
insert into public.geobr_main_levels
  (id, campaign_order, name, creator, description, difficulty, stars, song, platformer, gmd_path, file_version, published, coins)
values
  (2, 2, 'Teoria Tropical', 'Equipe GeoBR',
   'Fase original na musica Theory of Everything: cubo, nave, UFO, bola e dois trechos de Arrow.',
   'Hard', 5, 'Theory of Everything - DJ-Nate', false, 'main-levels/geobr-teoria-tropical-v1.gmd', 1, true, 0),
  (3, 3, 'Bloqueio Total', 'Equipe GeoBR',
   'Fase original na musica Deadlocked: tres trechos de Arrow e velocidades de 0.5x a 2x.',
   'Harder', 7, 'Deadlocked - F-777', false, 'main-levels/geobr-bloqueio-total-v1.gmd', 1, true, 0)
on conflict (id) do update set
  campaign_order = excluded.campaign_order, name = excluded.name, creator = excluded.creator,
  description = excluded.description, difficulty = excluded.difficulty, stars = excluded.stars,
  song = excluded.song, platformer = excluded.platformer, gmd_path = excluded.gmd_path,
  file_version = excluded.file_version, published = excluded.published, coins = excluded.coins,
  updated_at = now();

-- 3) RC8 release row.
insert into geobr_private.releases(version, enabled, message)
values ('1.9.0-beta.8', true, 'GeoBR RC8')
on conflict (version) do nothing;
