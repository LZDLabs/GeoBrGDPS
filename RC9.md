# GeoBR RC9 — 1.9.0-beta.9 (Android arm64)

Pacote: `dist/geobr.geobrmod.geode` — instale pelo Geode por cima da RC8. Geode 5.9.0, GD 2.2081. Dependências inalteradas.
SHA-256 do pacote: `caf38a4a54893ef3fd0743e47707030ea84ff400c2d624faf234a5498b13557b`

> Nada foi jogado no Geometry Dash (não há jogo/emulador neste ambiente). Compila sem avisos, linka contra o loader real e passa nos testes locais. O roteiro de teste no aparelho está no fim.

> **Antes de instalar:** aplique a migração `supabase/migrations/20261107120000_geobr_rc9_collab_groups.sql`. Ela inclui a RC8 (fases e beta.8), cria grupos e collab e libera a beta.9. O Supabase pediu sua confirmação duas vezes e ninguém respondeu, então **nada do servidor da RC9 está aplicado ainda**.

## Arrow em 90° no ar

O desenho do Arrow ficava dentro do nó do jogador. No modo Wave o GD gira esse nó sozinho, então o giro do GD somava com o nosso e dava 90° no ar. Agora o Arrow fica ao lado do jogador, na camada de objetos: acompanha a posição dele a cada quadro e o ângulo vem só da linha de 45° do Arrow. A física de chão e rampa da RC8 continua igual: reto no chão, segue a rampa, ±45° no ar. Com a gravidade invertida o desenho é espelhado, como os ícones do GD.

## God Mode: objetos aparecem na hora

Na RC8, os objetos só apareciam depois de sair do God Mode, quando a fase reiniciava e o GD remontava as seções. Agora, ao colocar um objeto, o mod repete o passo do carregamento da fase: reordena as listas de seção do GD. Se no quadro seguinte o objeto ainda não estiver desenhado, ele entra na seção dele; por último, é ativado direto. Sem o jogo não dá para ver qual desses passos o GD precisa. Se ainda não aparecer, me avise.

## Editor Collab

**Como usar**

1. **Criar uma sala.** Vá em Início → **Collab** → **Criar sala**. Comece de um level seu ou de um level vazio. A sala usa uma cópia, e o seu level original não muda.
2. **Convidar.** A sala gera um **código de 8 letras/números**, que você pode copiar.
3. **Entrar.** Outra pessoa abre Collab, digita o código e toca **Entrar**.
4. **My Collabs.** Depois de criar ou entrar numa sala, aparece em **Meus Levels** o botão **My Collabs** (dois cubos do GD com martelo, acima do botão New). Ele abre as suas salas.
5. **Editar.** Toque em **Abrir level**: o mod baixa a versão mais recente e abre a página normal do GD (Editar/Jogar).
6. **No editor:** botões **Sync** e **Chat** no topo.
   - **Sync** envia as suas mudanças, recebe as dos outros e aplica no editor **sem recarregar**: objetos removidos pelos outros somem e os novos aparecem.
   - **Chat** é o chat da sala, que atualiza sozinho.
   - **Salvar** também envia. Se alguém mudou ao mesmo tempo, aparece um aviso para usar o Sync.
7. **Publicar.** Quando um membro sobe o level pelo GD, o mod liga o ID do level à sala. Na lista de levels e na página do level aparece um **"+"** ao lado de "By …". Tocar mostra quem ajudou, com os ícones de cada um.

**Como as edições se juntam**

A fusão é por objeto, a partir da última versão sincronizada:
- o que cada pessoa **adicionou** fica;
- o que cada pessoa **apagou** sai;
- mover um objeto conta como apagar e adicionar.

As configurações do level ficam com quem as mudou; se os dois mudaram, fica a versão de quem sincroniza. Há 19 verificações de host em `tests/CollabMergeTests.cpp`.

**Limites**
- 8 pessoas por sala.
- 5 salas ativas por dono e 20 salas por pessoa.
- 8 MiB por level.
- 10 códigos errados por hora, para ninguém adivinhar códigos.
- Chat: 200 caracteres por mensagem, intervalo de 2 s.
- Se o dono sai, a sala passa para o membro mais antigo.

As 7 salas antigas do beta 1.3 continuam no banco (são as mesmas tabelas).

## Jogadores e grupos

- **Skin de cada jogador configurada certinho.** O mod agora sincroniza o ícone inteiro (cubo, cores, **glow** com a cor do glow e nave/bola/UFO/wave/robô/aranha/swing/jetpack) e mostra o glow nos avatares de toda a interface. Na RC7 os avatares dentro de cartões de tema ficavam invisíveis (a máscara de recorte); isso foi corrigido na RC8.
- **Cartão do jogador.** Tocar em alguém em Jogadores abre o cartão com todos os ícones e o Arrow, mais os botões **Mensagem** e **Adicionar a grupo**.
- **Grupos.** Em Jogadores → **Grupos** você:
  - cria um grupo (nome com 3 a 32 caracteres);
  - adiciona jogadores (quem foi adicionado recebe uma notificação);
  - vê os membros, e o dono pode remover membros;
  - usa o **chat do grupo** (300 caracteres, intervalo de 3 s, atualiza a cada 4 s).

  Quem bloqueou o outro nas DMs não pode ser adicionado. Limites: 30 membros por grupo, 10 grupos criados e 30 grupos por pessoa.
- Início ganhou o atalho **Collab** (7 atalhos agora).

## Não quebrar o GD e outros mods

Todas as telas novas são popups próprios. Nos ganchos do GD o mod só **acrescenta** menus próprios (IDs `geobr-*`) e nunca move, esconde ou reorganiza nós nativos; o `static_validation.py` verifica isso nos ganchos do collab. Onde o mod acrescenta algo:
- **Meus Levels:** só quando você tem sala.
- **Editor:** só em levels de collab.
- **"+" nos levels:** só nos levels publicados por um collab.
- **Upload e salvar no editor:** só agem em levels de collab.

A marca de collab vai no cabeçalho do level (chave `kGBC`). O GD sem o mod ignora essa chave.

## Servidor (migração pendente)

`20261107120000_geobr_rc9_collab_groups.sql`:
- reaproveita as tabelas de collab do beta 1.3;
- tabelas novas em `geobr_private` com RLS ligado;
- só funções com checagem de membro;
- execução apenas para `authenticated`;
- a RC8 vai embutida e é idempotente.

O teste ao vivo `tests/live_rc9.sql` está pronto. Ele roda em transação desfeita no final e cobre:
- ícones e glow;
- grupos: criar, adicionar, chat, limite de envio, quem está de fora, troca de dono;
- collab: código, bloqueio de tentativas, pull/push com conflito, chat, publicar, créditos, sair;
- permissões.

**Ainda não foi rodado**, porque o ensaio pediu sua confirmação no Supabase e não houve resposta.

## Testes executados (resultados reais)

| Teste | Resultado |
|---|---|
| Build Android arm64 Release limpo (NDK r28c, SDK 5.9.0), `--no-undefined` contra o loader real | PASS, 0 warnings |
| Host: **CollabMerge (19)**, ArrowPhysics (268), TextSanitizer (11), Models (20), ShopOrder | PASS |
| `static_validation.py`: beta.9, migração RC9, ícone do collab, ganchos só com menus `geobr-*` | PASS |
| `android_symbols.py`: 63 funções do GD usadas (ganchos do collab, editor, God Mode, glow), todas exportadas no Android 2.2081 | PASS |
| `package_v1.py`: manifesto beta.9, ELF AArch64, 99 recursos idênticos | PASS |
| `campaign_levels.py` (fases da RC8) | PASS |

**NÃO executado:**
- o ensaio da migração RC9 e o `live_rc9.sql` (o Supabase pediu confirmação e não houve resposta);
- o GD no aparelho.

## Limitações reais

1. **O collab não é em tempo real.** As mudanças dos outros chegam no **Sync** (ou ao abrir o level). Se duas pessoas mexem no mesmo objeto ao mesmo tempo, podem ficar as duas versões.
2. **A ligação "level publicado ↔ sala" vem do aparelho de quem publicou.** O servidor não consegue consultar os servidores do GD para confirmar. Cada ID só pode ser ligado a uma sala.
3. **Correções sem confirmação no jogo.** As do Arrow e do God Mode seguem a análise do código e não foram vistas na tela.
4. **Push para o GitHub continua bloqueado (403).**

## Checklist no aparelho

1. Arrow no ar: ±45° subindo e descendo; reto em cima de bloco; segue rampa; gravidade invertida.
2. God Mode: colocar objeto → aparece na hora.
3. Collab:
   - criar sala, copiar o código e entrar com outra conta;
   - "My Collabs" aparece em Meus Levels;
   - abrir, editar, salvar e usar Sync nos dois aparelhos;
   - o Chat do editor funciona;
   - subir o level e ver o "+" com os créditos.
4. Jogadores: avatares com glow; cartão com todos os ícones.
5. Grupos: criar, adicionar alguém, chat do grupo, sair.
6. GD normal: Meus Levels, editor e lista de levels sem sobreposição ou botões fora do lugar, inclusive com outros mods.
