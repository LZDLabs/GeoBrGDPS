-- GDPS 1.12.0: event levels carry their rewards ("type,amount,..." in GD's
-- SpecialRewardItem codes). Daily/Weekly become a queue (no schema change).
alter table geobr_gdps.timely add column if not exists rewards text not null default '';
