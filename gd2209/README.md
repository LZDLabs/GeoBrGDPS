# GeoBR para GD 2.2091 (Geode 6.0.0)

Build do **GeoBR 1.17.0** (com o GeoBR Natal 2026 e o collab profissional) e do
**GeoBR GDPS 1.0.3** para quem esta no GD 2.209/2.2091 com o Geode 6.0.0.

Downloads (pasta propria; os links do GD 2.2081 nao foram tocados):

- https://nolfgjsnnleqpmzvmebo.supabase.co/storage/v1/object/public/geobr-downloads/gd2209/geobr.geobrmod.geode?download
- https://nolfgjsnnleqpmzvmebo.supabase.co/storage/v1/object/public/geobr-downloads/gd2209/geobr.gdps.geode?download

| arquivo | versao | sha256 (inicio) |
|---|---|---|
| gd2209/geobr.geobrmod.geode | 1.17.0 | 20bbcb0e4c2e |
| gd2209/geobr.gdps.geode | 1.0.3 | ea375044dfa1 |

## Diferencas para a versao 2.2081

- `geode` 6.0.0, `gd` android/win 2.2091.
- Sem as dependencias `hjfod.gmd-api` e `hiimjustin000.more_icons` (o indice do Geode
  nao estava acessivel para conferir versoes para o Geode 6): o .gmd e lido/gravado pelo
  proprio DS_Dictionary do GD (`src/compat/LocalGmd.hpp`); sem More Icons o perfil mostra
  o icone normal do GD e o pacote de robos GeoBR nao entra no icon kit.
- Atualizacao automatica propria: este build le `updates/gd2209/latest.json` (os pacotes
  2.2081 continuam em `updates/latest.json`). A build 1.15.3 antiga desta pasta tinha o
  atualizador desligado, entao quem esta nela atualiza pelo link uma vez.
- Ajustes de API do Geode 6/GD 2.2091: `gd::string` -> `std::string`, caminhos
  `asp::fs::Path`, `TextInput` novo, `PlayLayer::init/scene` com 2 argumentos,
  `compressString` com nivel, `save(bool)`; no Windows o `EditorPauseLayer::saveLevel`
  e inline, entao o collab salva pelos botoes Salvar / Salvar e Jogar / Salvar e Sair.

## Como refazer

Aplicar `geobr-1.17.0-gd2209.patch` sobre o codigo 1.17.0 (LZDLabs/GeoBrGDPS, commit
"GeoBR 1.17.0") e compilar com o SDK do Geode `v6.0.0` (+ `Geode.lib` / `Geode.dll` /
`Geode.android64.so` do release v6.0.0) e as bindings atuais. Windows e Android precisam
de `-DFMT_MODULE=OFF`. O patch antigo `geobr-1.15.3-gd2209.patch` fica para referencia.
