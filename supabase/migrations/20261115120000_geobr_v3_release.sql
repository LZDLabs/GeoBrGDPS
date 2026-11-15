-- GeoBR 1.11.0: GD's own moderator rating (feature 0-4), no Demon List in the
-- mod, save separation through GManager (PC and Android), mod updates from the
-- release repository. No schema change; only the release row (non-destructive).
insert into geobr_private.releases(version, enabled, message)
values ('1.11.0', true, 'GeoBR 1.11.0')
on conflict (version) do nothing;
