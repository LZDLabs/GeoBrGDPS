-- GeoBR 1.14.3: crash reports from the live collab editor. When a session
-- ends in a crash, the mod sends its step trace and the tail of the previous
-- Geode log (tokens removed) once at the next start. Only the server reads
-- them (private schema). Non-destructive: new table and functions.

create table if not exists geobr_private.client_reports (
    id bigint generated always as identity primary key,
    user_id uuid references auth.users(id) on delete set null,
    kind text not null check (char_length(kind) between 1 and 40),
    version text check (char_length(version) <= 40),
    body text not null check (octet_length(body) <= 65536),
    created_at timestamptz not null default now()
);
create index if not exists geobr_client_reports_user_idx on geobr_private.client_reports(user_id, created_at);
alter table geobr_private.client_reports enable row level security;
revoke all on geobr_private.client_reports from public, anon, authenticated;

create or replace function geobr_private.client_report(p_kind text, p_version text, p_body text)
returns jsonb language plpgsql security definer set search_path = '' as $$
begin
 if auth.uid() is null then raise exception 'Sessao necessaria'; end if;
 if (select count(*) from geobr_private.client_reports where user_id = auth.uid() and created_at > now() - interval '1 day') >= 20 then
  return jsonb_build_object('ok', false, 'reason', 'limite diario');
 end if;
 insert into geobr_private.client_reports(user_id, kind, version, body)
 values (auth.uid(), left(coalesce(p_kind, 'report'), 40), left(p_version, 40), left(coalesce(p_body, ''), 60000));
 return jsonb_build_object('ok', true);
end $$;
revoke all on function geobr_private.client_report(text, text, text) from public, anon;
grant execute on function geobr_private.client_report(text, text, text) to authenticated;

create or replace function public.geobr_client_report(p_kind text, p_version text, p_body text)
returns jsonb language sql set search_path = '' as $$ select geobr_private.client_report(p_kind, p_version, p_body) $$;
revoke all on function public.geobr_client_report(text, text, text) from public, anon;
grant execute on function public.geobr_client_report(text, text, text) to authenticated;

insert into geobr_private.releases(version, enabled, message)
values ('1.14.3', true, 'GeoBR 1.14.3: relatorio de crash do collab')
on conflict (version) do nothing;

-- 1.14.4: entering a collab room without crash.
insert into geobr_private.releases(version, enabled, message)
values ('1.14.4', true, 'GeoBR 1.14.4: entrar no collab sem crash')
on conflict (version) do nothing;

-- 1.14.5: collab load after the editor settles (BetterEdit / start pos mods).
insert into geobr_private.releases(version, enabled, message)
values ('1.14.5', true, 'GeoBR 1.14.5: collab compativel com BetterEdit/StartPos')
on conflict (version) do nothing;

-- 1.14.6: music upload from the mod folder (phones without file picker).
insert into geobr_private.releases(version, enabled, message)
values ('1.14.6', true, 'GeoBR 1.14.6: enviar musica pela pasta no celular')
on conflict (version) do nothing;
