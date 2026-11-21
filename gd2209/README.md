# GeoBR para GD 2.2091 (Geode 6 beta)

Build separado do **GeoBR 1.15.3** e do **GeoBR GDPS 1.0.3** (a versao publica de
antes do Natal, sem nada do evento) para quem esta no GD 2.209/2.2091.

Downloads (pasta propria; os links do GD 2.2081 nao foram tocados):

- https://nolfgjsnnleqpmzvmebo.supabase.co/storage/v1/object/public/geobr-downloads/gd2209/geobr.geobrmod.geode?download
- https://nolfgjsnnleqpmzvmebo.supabase.co/storage/v1/object/public/geobr-downloads/gd2209/geobr.gdps.geode?download

| arquivo | sha256 (inicio) |
|---|---|
| gd2209/geobr.geobrmod.geode | 62de7302b2ce |
| gd2209/geobr.gdps.geode | 728e00439b3f |

## Diferencas para a versao 2.2081

- `geode` 6.0.0-beta.1, `gd` android/win 2.2091 (o Geode 6 beta so existe para 2.2091).
- Sem as dependencias `hjfod.gmd-api` e `hiimjustin000.more_icons` (nenhuma tem versao
  para Geode 6 ainda): o .gmd e lido/gravado pelo proprio DS_Dictionary do GD
  (`src/compat/LocalGmd.hpp`); sem More Icons o perfil mostra o icone normal do GD e o
  pacote de robos GeoBR nao entra no icon kit.
- Atualizacao automatica desligada (o `latest.json` aponta para os pacotes 2.2081).
- Ajustes de API do Geode 6/GD 2.2091: `gd::string` -> `std::string`, caminhos
  `asp::fs::Path`, `TextInput` novo, `PlayLayer::init/scene` com 2 argumentos,
  `compressString` com nivel, `save(bool)`; no Windows o `EditorPauseLayer::saveLevel`
  e inline, entao o collab salva pelos botoes Salvar / Salvar e Jogar / Salvar e Sair.

## Como refazer

Aplicar `geobr-1.15.3-gd2209.patch` sobre o codigo 1.15.3 (LZDLabs/GeoBrGDPS) e
compilar com o SDK do Geode `v6.0.0-beta.1` (+ `bin/nightly` do release beta.1) e
bindings atuais (com 2.2091). Windows precisa de `-DFMT_MODULE=OFF`.
Validadores do repo passam trocando 2.2081/5.9.0 por 2.2091/6.0.0-beta.1
(exceto a checagem do atualizador, que esta desligado de proposito).
