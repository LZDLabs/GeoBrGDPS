# GeoBR RC4 — 1.9.0-beta.4

Atualize o mod sem apagar os dados. Android64, GD 2.2081, Geode 5.9.0 ou compatível.

- Caixas: 4 / 5 / 6 / 7 / 8 / 10 itens da Comum à Mítica. Todos os itens aparecem numa grade de revelação.
- Tokens e Orbs garantidos; Rara+ garante um espaço de skin; Épica+ um espaço de título/tema exclusivo; Mítica tem dois espaços de skin e um exclusivo. Repetidos registrados viram 100 Tokens.
- Espaços aleatórios: 35% Tokens, 30% Orbs, 15% Skips, 12% skin, 4% título exclusivo e 4% tema exclusivo. Valores garantidos de Tokens/Orbs e drops por dificuldade são os mesmos da RC3. Skins de oito modos de jogo.
- Temas exclusivos: Aurora Boreal, Eclipse Rubro, Nebulosa Real e Cristal Astral. Títulos exclusivos: Cacador de Tesouros, Guardiao do Cofre, Sortudo e Reliquia Viva. Podem ser equipados na Tier Shop após ganhar; o servidor bloqueia sua compra, mesmo com preço zero.
- Aberturas anteriores são preservadas. Repetir uma abertura não concede novamente os itens.
- Missões: filtra eventos sem relação com o objetivo, prioriza os úteis e usa uma requisição para missão e drop. Fila verificada a cada segundo; passe atualizado após resposta em até um ciclo de 250 ms. A conexão ainda é necessária para confirmar XP/recompensas; não há prêmio apenas por previsão local.
- TOP: considera o nome da dificuldade quando ele é um sprite separado. Move rosto e nome juntos e posiciona a etiqueta abaixo do limite visual completo, com margem de 3 unidades.
- Janelas menores: até 78% da altura e 86% da largura disponíveis para passe, cofre, loja, comunidade, DMs, missões e ADM.

Validação: testes reais no backend com ROLLBACK para seis raridades, 40 itens, abertura repetida, caixas antigas, bloqueio de compra de exclusivos, equipamento de títulos/temas e missão sem XP duplicado. Testes de recursos, dimensões compactas e contrato de fila/etiqueta. Não há GDPS executando neste ambiente; confira o posicionamento do TOP e a legibilidade no aparelho, especialmente com outros mods de interface.

`database/rc4-catalog.sql` e `database/rc4-bundles.sql` são alterações já aplicadas ao projeto original. Não reaplique SQL em produção. Os demais arquivos SQL são histórico. Mesma senha ADM do servidor, sem senha embutida no mod.

Compilar com SDK Geode v5.9.0, bindings 7f6c2a75742856de88dad354e576dcff8a28e881, NDK r28c, CMake, Ninja e Geode CLI 3.9.0. Use os comandos do README e mantenha GEODE_SDK e GEODE_BINDINGS_REPO_PATH definidos também durante o build.
