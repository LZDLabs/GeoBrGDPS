> **RC7 (1.9.0-beta.7):** veja `RC7.md`. RC6: `RC6.md` e `BUILD-RC6.md`. Pacote: `dist/geobr.geobrmod.geode`.

# Versao atual: GeoBR V1 RC5

Leia [RC5.md](RC5.md) para instalar, usar o painel e conferir as mudancas desta entrega. Os demais relatorios preservam o historico de versoes anteriores.

# GeoBR RC4 — 1.9.0-beta.4

Consulte **RC4.md** para as alterações, probabilidades e validação desta versão. O restante deste arquivo registra a RC3.

# GeoBR V1 RC3 — 1.9.0-beta.3

Android arm64 · Geometry Dash 2.2081 · Geode 5.9.0 ou compatível.
Instale sobre o mod existente, sem apagar os dados.

## Nesta versão

- Cada Level Up pessoal, a partir do nível 2, libera uma recompensa permanente. XP vem das missões; a temporada muda sem apagar o nível pessoal. Prêmios anteriores podem ser resgatados, em sequência, em GeoPass > Levels.
- Notificação de Level Up; prévia das próximas seis recompensas e barra de XP pessoal.
- Comum, Incomum, Rara, Épica, Lendária e Mítica: seis imagens, inventário, abertura animada e recibo no servidor. Abra em GeoPass > Caixas.
- Por caixa: 40% Tokens, 30% Orbs, 10% Skips de missão, 15% skin nativa e 5% tema completo de perfil. Tokens = 20 × raridade²; Orbs = 100 × raridade²; Skips = max(1, floor(raridade/2)). Skins sorteadas entre Cube, Ship, Ball, UFO, Wave, Robot, Spider e Swing. Cosméticos já registrados como ganhos são convertidos em 100 Tokens. Ícones já desbloqueados por outros meios no GD não são consultados pelo servidor.
- Drop por conclusão no modo normal, com pelo menos 15 segundos jogados: Easy 10%, Normal 15%, Hard 20%, Harder 25%, Insane 35%, Demon 45%. Auto, Unrated, prática e editor não dão caixas. Até seis tentativas elegíveis por dia, em fases diferentes, com intervalo de 30 segundos. Dia contado em São Paulo.
- Ao ocorrer drop: Comum 60%, Incomum 25%, Rara 10%, Épica 4%, Lendária 0,9%, Mítica 0,1%. Não há compra de caixas por dinheiro.
- Passe atual recebe caixas extras nas etapas 5, 10, 15, 20, 25 e 30, sem mudar prêmios já resgatados. Bônus aparece no card da etapa.
- Níveis pessoais múltiplos de 5 dão caixa Épica; múltiplos de 10 dão Lendária; múltiplos de 25 dão Mítica. Outros níveis alternam Tokens e Orbs. Exigência acumulada: 100 × (nível − 1)² XP.
- Skips ganhos em caixas podem ser usados na tela da missão; não concedem XP pela missão pulada.
- Ranking de jogadores por XP permanente: GeoPass > Levels ou Caixas > Ranking; até 100 jogadores. TOP de fases é separado e definido no ADM.
- ADM > Ranking: confirmação explícita do ID e TOP salvos, edição do ID do registro existente, atualização imediata do cache. TOP abaixo da dificuldade nos detalhes e nas listas nativas, com espaço adicional no bloco da dificuldade. A tabela estava vazia; cadastre novamente o ID desejado.
- ADM > Gerar passe: nome, dias, etapas, missões e XP por etapa. Gere uma prévia, confira toda a lista e agende. A temporada atual é preservada. A próxima começa depois das temporadas já publicadas; configurações geradas podem ser editadas pelas abas do ADM antes do início, respeitando validações do servidor.
- Main Levels inicia a partida diretamente. Mapa permanece como "Em breve".
- Temas incluem o fundo do perfil nativo e motivos geométricos: fogo, gelo, floresta, cosmos, neon e ouro. Visibilidade pública requer o vínculo de conta verificado já existente no ADM e o mod no aparelho de quem visualiza.
- Corrigida a ordem das camadas: o painel marrom padrão encobria os fundos novos. Passe, loja, comunidade, DMs e ADM agora exibem o tema. Controles ajustados para telas horizontais pequenas; fonte nativa Pusab nos títulos e botões.

## Painel

Oito toques rápidos em More Games. A senha é validada no servidor e não é incluída na fonte nem no .geode. Sessão ADM de 20 minutos. Fechar o painel encerra essa sessão.

## Validação e limites

- Compilação Release AArch64, pacote e recursos verificados.
- Testes reais no backend com ROLLBACK: missões, XP, Tokens, resgates, prêmios de nível, abertura de caixa repetida, bloqueio de níveis futuros, Skips, privacidade das caixas/DMs, bloqueios, edição do TOP e geração/publicação de temporada. Usuários e temporadas de teste não permanecem.
- Checagens de limites de tela, espaçamento de cards, recursos com transparência e ordem de desenho dos fundos.
- Não há Geometry Dash executando neste ambiente. A entrada direta na fase, a aparência final e a convivência com os outros mods precisam ser verificadas no aparelho. RC3 não representa certificação de ausência de crashes.
- Autenticação anônima herdada: não apague os dados do mod. Eventos de gameplay são enviados pelo cliente; deduplicação, limites e recompensas são controlados no servidor, mas não constituem um anticheat completo.
- Notificações de DMs são dentro do jogo; não há push Android com o jogo fechado.

## Fonte e banco

`src/`: código C++ do mod. `resources/`: recursos empacotados. `database/`: alterações desta versão já aplicadas ao projeto original. `supabase/migrations/`: histórico anterior. `tests/`: validações locais e de backend. Não reaplique os arquivos SQL em produção.

As alterações de esquema foram aplicadas em blocos transacionais: crates_schema, progression_actions, season_builder, rank_edit_v3, crate_skin_variety e pass_skin_variety. Para atualizar uma cópia da RC2, `database/progression-v3.sql` seguido de `database/rank-edit-v3.sql` representam o estado final. `skin-pool-v3a.sql` e `skin-pool-v3b.sql` são histórico incremental já incorporado ao arquivo principal; não os reaplique junto dele.

## Compilar

Dependências: CMake 3.25+, Ninja, Geode CLI 3.9.0, SDK Geode v5.9.0, NDK r28c e bindings `7f6c2a75742856de88dad354e576dcff8a28e881`. Mantenha `Geode.android64.so` do SDK em `bin/5.9.0/`.

```bash
export GEODE_SDK=/caminho/geode
export GEODE_BINDINGS_REPO_PATH=/caminho/bindings
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/caminho/android-ndk-r28c/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j2
python3 tests/static_validation.py
python3 tests/progression_layout.py
python3 tests/package_v1.py build/geobr.geobrmod.geode
```
