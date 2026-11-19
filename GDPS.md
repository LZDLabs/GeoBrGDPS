# GDPS GeoBR — 1.13.0 (Android e PC)

São **dois arquivos**, instalados pelo Geode:

| Arquivo | O que é | SHA-256 |
|---|---|---|
| `dist/geobr.geobrmod.geode` | GeoBR Mod 1.11.0 | `c5b860008d7ec6d2a3564fb87b09553ac2f4ea34ddec6277c4663e8ecb889e27` |
| `dist/geobr.gdps.geode` | **GeoBR GDPS** 1.0.2 (troca de servidor e saves separados) | `0cecbba90d2bd6558da4ad0d1c8c92cec017ff78c613a270b7596f0bb2dff61e` |

> **Status do servidor (13/11):** a v2 está no ar. O banco foi atualizado, a função `gdps` está na versão 2 e a conta **Luiz** (ID 1, Owner) foi criada. O teste ao vivo contra o Supabase real passou. Os dados do teste de 08/11 foram **escondidos, não apagados**: as contas `GeoBRTeste` estão banidas e fora do ranking, e o level de teste está marcado como apagado. Assim a migração não precisou de nenhuma confirmação sua.

> **Correção 1.0.1 do GeoBR GDPS:** a versão 1.0.0 não carregava no aparelho (`library "hjfod.gmd-api.android64.so" not found`). O script de build ligava nela, sem necessidade, uma biblioteca do mod principal. A 1.0.1 só usa o Geode e as bibliotecas do sistema. Um teste novo (`tests/elf_needed.py`) lê as bibliotecas exigidas pelo `.so` e falha se houver alguma de um mod que não está nas dependências.

## 1.15.2 (19/11)

- Atualização automática no Android: o launcher do Geode carrega o binário (.so) do mod de uma pasta própria, não de dentro do .geode. O atualizador só trocava o .geode, então o jogo continuava com o binário antigo. Agora ele faz igual ao instalador do próprio Geode: também coloca o binário novo em `unzipped/binaries`, que é usado no próximo início.
- Se o pacote instalado for mais novo que o binário rodando (atualização feita por uma versão antiga do atualizador), o mod prepara o binário sozinho e pede para fechar e abrir o jogo.

## 1.15.1 (19/11)

- "Levar pro GDPS" e "Meus levels" (painel de Main Levels) davam "No file type set": o mod agora exporta o `.gmd` pela função pronta do gmd-api, já no formato GMD.
- O botão do GD normal virou um botão redondo "GDPS" na coluna da direita da página do level (com node-ids entra no layout da coluna), sem ficar por cima dos botões do GD e de outros mods.

## 1.15.0 (19/11)

- **Collab:** o host e os ADMs sincronizam todas as configurações da fase ao vivo: música, offset/fade, modo de jogo inicial, velocidade, mini, dual, espelhado, 2 jogadores, plataforma, fundo, chão, fundo do meio e fonte. Para os outros, a janela de configurações fica cinza (só o OK funciona) e qualquer mudança volta para a da sala. Botão "Membros" no editor do collab (o host dá/tira ADM ali). Mudanças em start pos e outros objetos entram assim que ninguém está com eles selecionados (não esperam mais janelas abertas). Botão "Convidar" na sala: a pessoa (usuário do GDPS) recebe uma notificação com o código.
- **Status online:** bolinha ao lado do nome no perfil — verde (online, até 5 min), cinza (inativo, até 3 dias), vermelha (fora há mais tempo) — e "visto há X". Configuração do mod "Quem vê quando estou online": Todos / Amigos / Ninguém. O mod avisa o servidor a cada 2 min.
- **Missões novas (sociais):** seguir todos os Elder Moderators, colocar 50 objetos num collab, comentar em 2 fases, fazer um amigo, curtir 3 fases, mandar uma mensagem, votar na dificuldade de 2 fases. Contam sozinhas pelo servidor (gatilhos nas tabelas do GDPS); a de seguir é conferida pelo mod (o GD guarda os "seguir"). O painel de quests aceita esses tipos.
- **Notificações:** mensagem recebida, pedido de amizade e convite de collab aparecem no jogo.
- **Main Levels (painel):** além de "Arquivo .gmd", agora "Meus levels" (escolhe um level seu; o mod gera o .gmd) e "Da pasta" (`geode/config/geobr.geobrmod/levels`). O envio para o Supabase continua automático.
- **GD normal → GDPS:** no GD normal (servidores oficiais), o seu level tem o botão "Levar pro GDPS"; a cópia aparece em Criar quando você abre o jogo no GDPS GeoBR.
- Servidor: migrações `20261119120000_geobr_social_presence.sql` e `20261119130000_geobr_social_triggers.sql` (sem apagar dados).

## 1.14.6 (16/11)

- Enviar música no celular: além de "Escolher arquivo" (seletor do sistema, que não abre em alguns celulares/launchers), o envio tem o botão "Da pasta": usa o MP3 mais recente da pasta `geode/config/geobr.geobrmod/musicas` (no Android: `Android/media/com.geode.launcher/game/geode/config/geobr.geobrmod/musicas`, acessível por qualquer gerenciador de arquivos). Se a pasta estiver vazia, o mod mostra o caminho e copia para a área de transferência.
- Se o seletor do sistema falhar, a mensagem mostra o erro exato.

## 1.14.5 (15/11)

- Crashlog do celular (ainda na 1.14.3): o crash era um "double free" dentro do gancho `removeSpecial` do BetterEdit (com Clean StartPos e Tinker no caminho), chamado pela remoção direta do collab (`removeObject` sem desfazer, logo ao abrir o editor). A 1.14.4 já apaga pelo caminho do jogador (com desfazer, objeto segurado até o fim); a 1.14.5 também só carrega a sala quando o editor terminou de abrir (transição acabou) e passou ~2 s, para os outros mods prepararem o editor antes.

## 1.14.4 (15/11)

- Crash ao entrar na sala corrigido (achado pelos relatórios automáticos, Android e PC): o crash acontecia ao remover, com `LevelEditorLayer::removeObject`, um objeto do level local fora da tela (start pos, portal). Agora o collab apaga como o jogador (seleciona e usa Delete do GD), mantendo a seleção de quem está editando; vale para a entrada na sala e para as mudanças ao vivo.
- Na entrada, objetos iguais com diferença só de casas decimais (ex.: rotação -23 e -23.02) contam como iguais e não são trocados.

## 1.14.3 (15/11)

- Ao abrir uma sala, objetos que já estão iguais no seu editor só recebem o id da sala (antes todos eram apagados e criados de novo); só o que é diferente é trocado.
- Relatório automático de crash do collab: cada passo do editor ao vivo vai para `collab-trace.txt` (pasta de saves do mod). Se o jogo fechar no meio, na próxima abertura o mod envia esse registro e o fim do log anterior do Geode (sem tokens/senhas) para o servidor, uma vez. Tabela `geobr_private.client_reports` (só o servidor lê). Migração `20261115160000_geobr_client_reports.sql`.

## 1.14.2 (15/11)

- Collab ao vivo mais seguro (o host fechava ao entrar com gente na sala): as mudanças dos outros só são aplicadas no início de um quadro (como um toque), nunca no meio das atualizações do editor; no máximo 400 por vez; esperam enquanto você arrasta, seleciona por área, move a câmera ou toca a música do editor.
- A música recebida não chama mais a rotina interna de "configurações atualizadas" do GD (só muda os campos da música); o botão de desfazer não é mais atualizado à força.
- Os nomes dos jogadores e o status ficam dentro de um único nó do collab na interface.

## 1.14.1 (15/11)

- Salvar/criar sala collab: corrigido o erro "unsupported Unicode escape sequence". O level comprimido pelo GD às vezes saía com um byte nulo, que o servidor recusa. Agora os bytes nulos são removidos, a compressão é conferida antes do envio e, se falhar, o level vai sem compressão (todas as versões leem os dois formatos).
- Registro (log do Geode) das etapas do collab ao vivo, para achar a causa se o jogo fechar.

## 1.14.0 (15/11, correção do collab)

- Editor collab sem crash: as mudanças dos outros não são aplicadas durante o playtest, nem em objetos que você está selecionando/editando, nem com uma janela aberta (ficam na fila e entram depois). O histórico de desfazer é limpo quando a sala troca um objeto. Os nomes dos jogadores ficam numa camada da interface (antes ficavam dentro da camada de objetos do level).
- Música no collab: quando o host (ou um ADM da sala) escolhe a música, offset ou fade nas configurações do level, a mudança vai para todos ao vivo. O servidor só aceita isso do host/ADMs e não envia para quem está na 1.12/1.13.
- O status de ADM da sala atualiza durante a edição (não precisa sair e entrar).
- Servidor: migração `20261115150000_geobr_collab_settings.sql` (só substitui funções; nenhum dado apagado).

## 1.13.0 (15/11, versão final)

| Pedido | Feito | Testado |
|---|---|---|
| Skin no chat | O Chat Geral, o chat do collab e o de grupo mostram a skin **atual** de quem mandou (cubo, cores e glow), lida do perfil. | Servidor: o feed traz glow e ícones |
| Apagar as mensagens | O Chat Geral foi esvaziado (0 mensagens). | Produção |
| GeoBR só no GDPS | No servidor Oficial todos os hooks do GeoBR ficam desligados, menos o botão de servidor (e a busca de atualização): o GD fica vanilla. | Compila; falta testar no jogo |
| Um precisa do outro | O GeoBR exige o GeoBR GDPS como dependência no Geode. O GeoBR GDPS só ativa o GDPS se o GeoBR estiver instalado e ativo. | Pacote: dependência declarada |
| Painel antigo (More Games) no painel do dono | O atalho dos 8 toques em "More Games" saiu. Agora é Painel ADM → Resumo → **Painel GeoBR** (só o Owner), sem senha: a conta GDPS do Owner abre a sessão de 20 minutos. | Produção (transação desfeita): senha errada recusada, Owner abre |
| Só o primeiro Main Level | Fica só "Loucura Brasileira". "Teoria Tropical" e "Bloqueio Total" foram despublicados, não apagados. | Produção |
| IDs separados nos packs | Map Pack: um campo para cada um dos 3 levels. Gauntlet: tipo + 5 campos. Formulários grandes ficam em 2 colunas. | Compila |

## 1.12.0 (15/11)

| Pedido | Feito | Testado |
|---|---|---|
| Collab ao vivo (sem "Sync") | O level da sala fica guardado objeto por objeto no servidor. Cada editor envia o que mudou e recebe o dos outros cerca de 1 vez por segundo. O nome de cada pessoa aparece no ponto do level onde ela está. O botão Sync saiu e o Chat ficou. | Servidor em produção, com transação desfeita no final: semear a sala, edição e exclusão nos dois sentidos, presença. Editor: compila em Android e PC; falta testar no jogo |
| Só o host e os ADMs salvam | O host marca ADM em Membros ("Dar ADM"). Salvar o level da sala e publicar ficam só para host e ADMs, e o servidor recusa os outros. | Produção (membro recusado, ADM salva) |
| Rate confuso | Popup no estilo da RobTop: estrelas de 1 a 10; tocar no rosto troca Rated > Featured > Epic > Legendary > Mythic; dificuldade de Demon e moedas. É o mesmo no botão Mod do level e no Painel ADM. | Compila; falta testar no jogo |
| /daily e /weekly | Comando no comentário do level (só ADM). Funciona como fila: um level por dia (UTC) ou por semana (segunda). Sem level na fila, não tem Daily. | Servidor local |
| Event | `/event 500 diamantes 1000 orbs`: o level fica como Event, com recompensas, até você definir outro. | Servidor local, incluindo a resposta codificada que o GD lê |
| Mudar o nome da conta | GDPS → Mudar nome (uma vez por dia; o GD guarda o nome novo para o login). | Servidor local |

## 1.11.0 (15/11)

| Pedido | Feito | Testado |
|---|---|---|
| Rate igual ao da RobTop | O botao de estrelas do GD (moderador). Tocar no rosto troca Rated > Featured > Epic > Legendary > Mythic. O servidor entende `feature` 0-4. O popup "Mod" ficou so com Tirar rate, Moedas, Daily/Weekly/Evento, Bloquear e Apagar. | Servidor local: 0-4 viram key 42 = 0,1,2,3 |
| Apagar Demon List | Removida do menu GDPS, do painel ADM, do popup Mod e do comando `!demonlist`. Os dados no banco ficam guardados. | `static_validation.py` |
| Collab nao entrava com o codigo | Nenhum pedido de "entrar" chegou ao servidor: o botao saia em silencio enquanto outro pedido estava pendente. Agora usa um pedido proprio, mostra "Aguarde", libera depois de 20 s e aceita codigo com espaco ou traco. | Build; falta testar no jogo |
| Ja estava logado na conta | Os logs mostram o mod 1.9.0-beta.9 (RC9) em uso, sem separacao. A separacao agora troca o arquivo do save (`GManager::setup`) e funciona no PC e no celular. A pasta continua `geobr-gdps/`. | Build Android; falta testar no jogo |
| Musicas | Envio real em producao: criar, enviar o MP3, conferir, publicar, `getGJSongInfo` e arquivo publico 200. A musica de teste foi removida. | Ao vivo |
| Atualizacao automatica | No menu principal o mod le `updates/latest.json` de LZDLabs/GeoBrGDPS, pergunta, baixa, confere o SHA-256 e troca o pacote. O jogo reinicia. **O repositorio precisa ser publico** (privado responde 404). | SHA-256: 4 vetores FIPS |
| PC | **Bloqueado:** compilar para Windows exige baixar o MSVC/Windows SDK aceitando a licenca da Microsoft, e isso precisa da sua autorizacao. | - |

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
