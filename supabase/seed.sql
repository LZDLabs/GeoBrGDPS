insert into public.geobr_main_levels (
    campaign_order, name, creator, description, difficulty, stars, song,
    platformer, gmd_path, file_version, published
)
values (
    1,
    'Level de exemplo (nao publicado)',
    'Equipe GeoBR',
    'Substitua os dados, envie o .gmd e publique quando estiver pronto.',
    'Normal',
    3,
    'Musica de exemplo',
    false,
    'main-levels/exemplo.gmd',
    1,
    false
)
on conflict (campaign_order) do nothing;
