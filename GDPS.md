# GDPS GeoBR v2 — 1.10.0-beta.2 (Android arm64)

São **dois arquivos**, instalados pelo Geode:

| Arquivo | O que é | SHA-256 |
|---|---|---|
| `dist/geobr.geobrmod.geode` | GeoBR Mod 1.10.0-beta.2 | `12354dc8df646605ba5ecef2b7c66acc063dad60871c5d38b67e39af08be5965` |
| `dist/geobr.gdps.geode` | **GeoBR GDPS** 1.0.1 (troca de servidor e saves separados) | `95e02a8a4506d6f9d31f610f745d761c872cfeadb8c07a44e7d0a24f1cd1a380` |

> **Status do servidor (13/11):** a v2 está no ar. O banco foi atualizado, a função `gdps` está na versão 2 e a conta **Luiz** (ID 1, Owner) foi criada. O teste ao vivo contra o Supabase real passou. Os dados do teste de 08/11 foram **escondidos, não apagados**: as contas `GeoBRTeste` estão banidas e fora do ranking, e o level de teste está marcado como apagado. Assim a migração não precisou de nenhuma confirmação sua.

> **Correção 1.0.1 do GeoBR GDPS:** a versão 1.0.0 não carregava no aparelho (`library "hjfod.gmd-api.android64.so" not found`). O script de build ligava nela, sem necessidade, uma biblioteca do mod principal. A 1.0.1 só usa o Geode e as bibliotecas do sistema. Um teste novo (`tests/elf_needed.py`) lê as bibliotecas exigidas pelo `.so` e falha se houver alguma de um mod que não está nas dependências.

## O que mudou (e por quê)

Os logs do servidor mostraram que, no dia 08/11, o seu GD falou com o GDPS (listas e downloads chegaram), mas ninguém conseguiu se cadastrar. A conta oficial continuava no mesmo save do GD, então GDPS e GD oficial se misturavam. Agora é como num GDPS de verdade:

- **Saves e contas separados.** O mod GeoBR GDPS carrega antes do GD abrir o save:
  - **GD Oficial:** o GD usa o save de sempre, com a sua conta de sempre.
  - **GDPS GeoBR:** o GD usa outra pasta (`<save>/geobr-gdps/`), com save, conta, progresso, levels criados e levels salvos próprios.

  O save oficial não é aberto nem gravado no modo GDPS. Se você desinstalar o GeoBR GDPS, o GD volta ao normal.
- **Trocar de servidor (para nunca ficar preso):**
  - um botão no menu principal mostra o servidor atual (`Oficial`/`GDPS`);
  - também dá para trocar em Início → GeoBR → GDPS → **Trocar servidor**;
  - ou em Geode → GeoBR GDPS → Servidor.

  O jogo salva e reinicia sozinho. O padrão é **Oficial**.
- **Contas pelo próprio GD:** no modo GDPS, Conta → Registrar/Entrar, como no GD normal. O e-mail não é necessário.
- **IDs a partir de 1:** contas e levels do GDPS contam de 1 (sem os 900 milhões). A conta Luiz já é o ID 1, e o primeiro level publicado será o ID 1. Os IDs das músicas GeoBR continuam a partir de 8000001, porque precisam ser diferentes dos do Newgrounds.

## Recursos de um GDPS completo

| Recurso | Como funciona |
|---|---|
| Contas | Registrar e Entrar do GD, Save/Load na nuvem GeoBR, recuperação por código, **trocar senha** (Início → GDPS) |
| Levels | Publicar, buscar com todos os filtros, baixar, curtir, comentar, denunciar, Daily/Weekly/Evento |
| **Map Packs** e **Gauntlets** | Criados no Painel ADM → Packs; aparecem nas telas normais do GD |
| **Listas de levels** (GD 2.2) | Criar, buscar, curtir; a moderação dá estrelas e Featured |
| **Baús e missões** | Agora vêm do GDPS: baú pequeno a cada 4 h, grande a cada 24 h, 3 missões por dia |
| Ranking | Top, Criadores, Amigos, Global e ranking por level |
| Moderação pelo GD | Botão **Req** → o servidor responde Moderador ou **Elder Moderator**; o botão de rate do GD avalia direto |
| **Comandos nos comentários** | `!rate 5`, `!epic`, `!legendary`, `!mythic`, `!feature`, `!unrate`, `!coins`, `!daily`, `!weekly`, `!event`, `!demonlist 1`, `!block`, `!delete`, `!ban NOME`, `!mute NOME horas`, `!lbban NOME`, `!lbunban NOME`, `!help` (só para quem tem cargo) |
| **Botão "Mod"** | Na página do level e no perfil de um jogador (só para quem tem cargo); explicado abaixo |
| Painel ADM | Jogadores, Levels, Listas, Comentários, Eventos, Demon List, Músicas, Cargos, Ranking, Auditoria, **Packs** |
| Músicas | Biblioteca GeoBR (envio de MP3, moderação, IDs permanentes) |

**Botão "Mod":**
- **Na página do level:** rate, Featured/Epic/Legendary/Mythic, Daily/Weekly/Evento, Demon List, bloquear e apagar.
- **No perfil de um jogador:** banir, mutar comentários, tirar do ranking ou voltar ao ranking, zerar estatísticas, histórico e cargo.

## Sua conta Luiz (dono, Elder Moderator)

A conta **Luiz** (ID 1) já existe no GDPS, com o cargo **Owner**: todas as permissões e o selo de **Elder Moderator**. A senha provisória foi entregue no chat (não fica no repositório). Para usar:

1. troca o servidor para **GDPS GeoBR** (o jogo reinicia);
2. Conta → **Entrar** com `Luiz` e a senha provisória;
3. Início → GeoBR → GDPS → **Trocar senha** (escolha a sua);
4. no GD, toque em **Req** para ativar o selo e os botões de moderador. A partir daí:
   - você avalia levels pelo botão de rate do GD;
   - pelo botão "Mod" e pelos comandos `!rate` e `!epic`;
   - e bane do ranking pelo perfil ("Mod" → Tirar do ranking) ou com `!lbban NOME`.

## Testes executados (resultados reais)

| Teste | Resultado |
|---|---|
| Build Android arm64 Release limpo dos **dois** mods (NDK r28c, Geode 5.9.0, `--no-undefined`) | PASS, 0 erros, 0 warnings no código |
| Servidor local (Postgres 16 real, migrações v1+v2 aplicadas duas vezes) | PASS, **27 grupos**, incluindo os novos: Map Packs e Gauntlets com hash do GD, listas, baús e missões (codificação e hash do GD), comandos e mute, troca de senha, IDs começando em 1 |
| Migração v2 **aplicada em produção** (versão sem DELETE) | Esquema de produção idêntico ao local (262 colunas, 28 tabelas, 74 índices); dados de teste escondidos; contas e levels recomeçam em 1 |
| Deploy da função `gdps` versão 2 | Os 15 arquivos publicados conferem byte a byte com os locais; `info` responde `1.10.0-beta.2` |
| **Teste ao vivo v2 contra o Supabase real** (`tests/live_gdps_v2.py Luiz ...`) | PASS, 14 verificações: Luiz ID 1, Owner, **Req → 2 (Elder)**, selo no perfil, painel ADM, baú, missões, packs e listas |
| Leituras em produção | Contas de teste não aparecem na busca, o level 900000001 não abre, o ranking está vazio, e todas as respostas nos logs são HTTP 200, sem erros |
| Host: GdpsRouting (24), CollabMerge (19), ArrowPhysics (268), TextSanitizer (11), Models (20), ShopOrder | PASS |
| `android_symbols.py`: 78 funções do GD usadas (incluindo `CCFileUtilsAndroid::getWritablePath`, `ProfilePage`, `MenuLayer`) | PASS |
| `static_validation.py`, `package_v1.py`, `package_core.py` | PASS |

**Ainda NÃO executado:** o GD no aparelho (separação de saves, botões, Req e rate pelo jogo). Siga o checklist abaixo.

## Limitações reais

1. **Não foi visto no jogo.** A separação de saves usa o mesmo princípio do "GDPS Switcher" (carregar antes do GD e trocar a pasta de gravação), mas não foi testada num aparelho.
2. **O GDPS começa do zero:** ícones, progresso e levels salvos do GD oficial não aparecem no GDPS, e vice-versa. É o comportamento de um GDPS.
3. **Músicas e efeitos baixados** também ficam por servidor e são baixados de novo no GDPS.
4. **O código secreto do cofre** (vault) e a lista de artistas continuam na RobTop.

## Checklist no aparelho

1. Instale os **dois** arquivos. O menu deve mostrar o botão **Oficial**, e o GD continua com a sua conta normal.
2. Toque no botão → **GDPS GeoBR**. O jogo reinicia com save novo, sem conta.
3. Entre como Luiz, troque a senha e toque em **Req**.
4. Publique um level e use `!rate 5` nos comentários. Abra o botão **Mod** do level.
5. Painel ADM → Packs: crie um Map Pack e uma Gauntlet e veja-os nas telas do GD.
6. Abra os baús e as missões.
7. Volte para **Oficial**: a sua conta e o seu save de sempre devem estar intactos.
