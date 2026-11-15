# GeoBR RC7 — 1.9.0-beta.7 (Android arm64)

Pacote: `dist/geobr.geobrmod.geode` — instale pelo Geode por cima da RC6. Geode 5.9.0, GD 2.2081. Dependências inalteradas.
SHA-256 do pacote: `9af7d32a9c40b6a492468d3e354e371e637de8b038b8e88cd5f8a84844de606a`
SHA-256 do binário: `23c4b9e0e22181ff9362aa13623f1bb01b161759c2e37894abf67d85694f4cce`

> Como na RC6: nada aqui foi jogado no Geometry Dash (não há jogo/emulador neste ambiente). Compila, linka contra o loader real e passa nos testes automáticos e de servidor. O roteiro de teste no aparelho está no fim.

## Correções pedidas

**Arrow em ±90° no ar.** O ângulo do ícone era calculado pelo deslocamento na tela; no GD a câmera segue o jogador na horizontal, então na tela ele quase só sobe/desce (±90°). No chão o deslocamento era ~0 e caía no fallback de 45°. Agora o ângulo é definido no espaço da fase (±45° pela direção, espelhado indo para a esquerda) e convertido para a tela pela transformação da camada — funciona também com câmera girada/espelhada.

**Tela preta no Hub.** Na RC6 o fundo das janelas virou cor sólida; o Hub usa uma cor quase preta (30,31,54) e não tinha arte. Agora: fundo em gradiente iluminado (como os quadros do GD), arte Cosmos aprovada no Hub, e o contorno das molduras é traçado só nas bordas — antes era um polígono preto cheio por baixo, que escurecia os painéis translúcidos (notícias/chat). Cores do `CCDrawNode` passaram a ser pré-multiplicadas (é o blend que o cocos usa).

## Arrow no icon kit (estilo GD)

O atalho "Arrow" do Hub foi removido. O Arrow virou uma **aba na garagem do GD** (`GJGarageLayer`), logo após a última aba nativa, com o mesmo espaçamento. Tocar na aba esconde a grade nativa e mostra os Arrows em grade, com prévia grande (balançando ±45°) onde o GD mostra o ícone, cursor `GJ_select_001` na skin atual e cadeado `GJ_lock_001` nas bloqueadas (tocar mostra como obter; as da loja oferecem abrir a loja). Trocar para outra aba nativa restaura tudo. O botão do perfil continua.

## 16 skins (12 novas)

| # | Skin | Como obter |
|---|---|---|
| 1–4 | Flecha, Dardo, Pena, Cometa | Grátis |
| 5 | Raio | Loja, 80 Tokens |
| 6 | Tridente | Loja, 120 Tokens |
| 7 | Foguete | Loja, 160 Tokens |
| 8 | Origami | Loja, 200 Tokens |
| 9 | Bandeira | GeoPass, etapa bônus 31 |
| 10 | Coroa | GeoPass, etapa bônus 32 |
| 11 | Madeira | Exclusiva da Caixa Comum |
| 12 | Esmeralda | Exclusiva da Caixa Incomum |
| 13 | Safira | Exclusiva da Caixa Rara |
| 14 | Ametista | Exclusiva da Caixa Épica |
| 15 | Solar | Exclusiva da Caixa Lendária |
| 16 | Supernova | Exclusiva da Caixa Mítica |

Todas com camadas Col 1 / Col 2 / Glow / detalhe, geradas por `tools/generate_rc6_art.py`.

Servidor (migração `geobr_rc7_arrow_items`, **aplicada**): item `arrow` na loja (`arrow-5`…`arrow-16`) e aquisição `pass`; compra cobra uma vez e itens de passe/baú não são vendidos; `deliver` e `claim_reward` entregam Arrows (repetido vira 100 Tokens); cada baú sorteia o Arrow exclusivo da sua raridade no 3º espaço (8 % Comum … 30 % Mítica); etapas 31/32 adicionadas à temporada ativa "GeoBR - Expedição"; equipar valida posse (1–4 grátis); o estado marca o Arrow equipado. Cliente: aba **Arrows** na Tier Shop, recompensas e baús mostram o Arrow, formulários ADM aceitam `arrow` e `item_id`.

Temporadas futuras: o gerador de temporadas não foi alterado; para colocar Arrows de passe nelas, use ADM > Prêmios com tipo `arrow` e item `arrow-9`/`arrow-10` (ou novos itens).

## Ícone do mod

`logo.png` (336×336): o território do Brasil a partir da fronteira real (Natural Earth 1:50m, domínio público, extraída para `art-source/brazil-outline.json`), em verde com contorno preto, borda branca, luz amarela, losango e círculo da bandeira, sobre um quadro azul no estilo GD. Gerador: `tools/generate_logo.py`.

## Campanha: duas fases novas

Embutidas no mod (`resources/campaign/*.gmd`) e listadas primeiro em Main Levels; funcionam mesmo sem servidor e não dependem de upload no Storage.

| Fase | Música (oficial do GD) | Duração a 1x | Conteúdo |
|---|---|---|---|
| GeoBR Theory (Normal, 4★) | Theory of Everything (#11) | ~93 s | cubo, nave, **Arrow**, bola, Wave, UFO |
| GeoBR Deadlocked (Hard, 6★) | Deadlocked (#19) | ~106 s | cubo, nave, **Arrow ×2**, Wave, UFO, bola |

Geradas por `tools/generate_campaign.py` (determinístico). No trecho do Arrow, pilares alternam chão/teto a cada 10 blocos com corredores de 3–4 blocos (a 45°, 10 blocos na horizontal = 10 na vertical, então há folga), além de pequenos espinhos no chão. As regras de espaçamento foram feitas para dificuldade fácil/normal porque **as fases não foram jogadas**: ajuste fino de ritmo/decoração precisa ser feito no editor depois de testar.

## God Editor

No menu de pausa, ao jogar **uma fase sua** (nível local do editor), aparece o botão de bloco com martelo ("God"). Ele abre o modo criativo do GeoBR em qualquer modo (clássico ou plataforma):

- O jogador voa livre com D-pad na tela (←→↑↓), sem morrer e sem colisão; a fase não "termina".
- Zoom + / − (câmera do GD).
- **Objetos**: categorias Blocos, Perigos, Pulos, Portais (inclui o portal marrom do Arrow) e Deco, ou **qualquer objeto do editor pelo ID** (1–5000, validado no `ObjectToolbox`). Toque na tela para colocar, alinhado à grade de 30.
- **Cores**: Fundo, Chão, Chão 2, Linha, Objetos e canais 1–4, com 32 cores do GD, aplicadas ao vivo.
- **Salvar**: grava os objetos novos (pelo `getSaveString` do próprio GD) e as cores (como gatilhos de cor no início) na string do level e salva os levels locais.
- **Sair**: volta ao jogo normal reiniciando a tentativa, já com os objetos.

Não aparece em fases online/oficiais/campanha (só "testando sua fase").

## Testes executados (resultados reais)

| Teste | Resultado |
|---|---|
| Build Android arm64 Release limpo (NDK r28c, SDK 5.9.0), `--no-undefined` contra o loader real | PASS, 0 warnings |
| `package_v1.py`: ZIP, manifesto 1.9.0-beta.7, ELF AArch64, ícone e 100 recursos idênticos | PASS |
| `static_validation.py`: 16 skins espelhadas no servidor, 1 exclusiva por raridade, ícones, fases, unidades | PASS |
| `android_symbols.py`: 43 funções do GD usadas (Arrow, garagem, God Editor) exportadas no Android 2.2081 | PASS |
| `campaign_levels.py`: decodifica os `.gmd` como o GD (plist → k4 → base64 → gzip), músicas, portais, Arrow, limites | PASS |
| Host: ArrowPhysics (268), TextSanitizer (11), Models (20), ShopOrder | PASS |
| Ensaio da migração RC7 com ROLLBACK e `tests/live_rc7.sql` no banco real (ROLLBACK) | PASS |
| Advisors Supabase após a migração | 0 ERROR (mesmos avisos herdados) |

NÃO executado: GD no aparelho, toque real, visual, outros mods.

## Limitações reais

1. **God Editor** depende de `PlayLayer::addObject` aceitar objetos com a fase já iniciada (o mesmo caminho do carregamento). Objetos especiais (triggers complexos) podem não se comportar como no editor. Não há "desfazer"/apagar no God Editor: para remover, use o editor normal. Cores salvas viram gatilhos de cor no início da fase.
2. **Garagem**: a aba é posicionada após a última aba nativa por medição; se outro mod reorganizar a garagem, a posição pode ficar diferente.
3. **Fases**: geradas por código, sem teste de jogabilidade; corredores dos veículos seguem as regras do GD que não pude verificar no jogo (o gerador também desenha um teto de blocos, por segurança).
4. Upload das fases para o Storage não foi feito (o host do Storage é bloqueado neste ambiente); por isso elas vão dentro do mod.
5. Push para o GitHub continua bloqueado (403), como na RC6.

## Checklist no aparelho

1. Arrow no ar: ±45° subindo/descendo, em normal, mini, gravidade invertida e Dual.
2. Hub: abrir Início — fundo visível (sem tela preta), notícias/chat legíveis.
3. Garagem: aba do Arrow, trocar skin, voltar para cubo/nave; skins bloqueadas mostram como obter.
4. Loja > Arrows: comprar Raio, equipar; abrir caixas; GeoPass etapas 31/32.
5. Main Levels: GeoBR Theory e GeoBR Deadlocked com a música certa e o portal marrom.
6. Fase sua → pausa → God: voar, zoom, colocar blocos/portal Arrow, mudar cor, Salvar, Sair; reabrir no editor e conferir.
