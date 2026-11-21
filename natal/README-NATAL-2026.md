# GeoBR Natal 2026: estado atual (1.16.8, teste, NÃO publicada)

O evento fica só no GDPS GeoBR. Nada foi publicado para os jogadores: o `latest.json`
do repositório de releases continua no 1.15.3.

## Como fica

- **Datas:** abre para todos em **1/12 00:00 (Brasília)** e fecha em 27/12 00:00
  (`natal_config.starts_at` / `ends_at`). Para isso, "Evento para todos" precisa estar
  LIGADO no Menu do Dono.
- **Calendário:** o dia N abre em N de dezembro, de 1 a 25. Dá para abrir dias
  atrasados (`calendar_catchup`).
- **Modo teste da staff:** só vale antes de 1/12. O "dia de teste" também.
  Na abertura, o que a staff fez testando para de contar, na primeira vez que cada um
  entra:
  - zera: roleta, calendário, passe e Candy, recontados a partir de 1/12;
  - fica: acessórios, temas e molduras.

  Nada é apagado: no histórico (`natal_ledger`), os registros de teste ganham o
  prefixo `testeAAAAMMDD_`.
- **Fase do Evento:** as fases que o Dono publica entram em rodízio, trocando a cada X
  horas, com contagem regressiva e Candy uma vez por fase.
- **Batalha Natalina (só clássico):**
  1. O Dono abre (ou agenda) a batalha e todos entram durante a espera (1 min).
  2. Cada um vota numa das fases clássicas publicadas.
  3. **Quem passar a fase primeiro ganha** e a batalha acaba na hora.
  4. Se ninguém passar no tempo (hoje 3 min, ajustável no Menu do Dono), vence quem
     chegou mais longe.
  5. Prêmios em Candy para o top 3. O modo prática não conta. BattleGuard continua.
- **Batalha automática:** toda hora cheia (Brasília) tem batalha, com entrada 1 min antes.
  Configura no Menu do Dono → Evento: desligada, 30 min, 1 h, 2 h ou 3 h. A "fase do evento"
  saiu; a aba Evento mostra a próxima batalha. A batalha automática avisa com 5 min e 1 min.
- **Depois de passar:** o 1º a passar vence na hora, os outros continuam pelo 2º/3º.
  Quem passou pode **Espectar** (todos com %, ao vivo) ou **Jogar de novo** (não muda nada).
  Modo prática bloqueado na fase da batalha. Candy para quem jogou: +40 por passar,
  +15 por chegar a 50% (além do top 3).
- **Passe:** 40 níveis até 4500 Candy, pelo **Candy que você tem agora** (gastar também
  baixa o passe). Candy mais difícil: top 3 60/35/20, +15 por passar, +5 por 50%, roleta menor.
- **Espectar a tela:** cada jogador manda a posição 10x por segundo (`natal_arena_live`);
  quem espectar vê o ícone do outro andando na fase (1,6 s atrás) e a câmera segue ele
  (`natal_arena_watch`). Dá para trocar de jogador (< >) e voltar a jogar. Pelo Hub,
  "Espectar a tela" abre a fase só para assistir.
- **Bots:** ninguém coloca bot. Só quem ficar sozinho na batalha ganha 1 bot "burro": ele
  faz tentativas (morre em pontos diferentes, recomeça) e raramente passa.
- **Ao passar a fase:** sempre aparece a janela com a posição e os Candy ganhos
  (ou "os Candy saem quando a batalha acabar") e Espectar / Jogar de novo.
- **Acessórios:** posição calculada pela arte de cada um (`tools/natal_accessory_placement.py`);
  cachecóis e pisca-pisca com arte nova que envolve o ícone; no robô/aranha ficam na cabeça,
  na nave/UFO/jetpack no cubinho de dentro, escondidos na onda/swing.
- **Notícias e avisos com imagem:** Painel ADM → Notícias/Avisos → "Imagem: arquivo" ou
  "Imagem: da pasta" (pasta `imagens` do mod). Aviso com imagem abre como janela para todos;
  notícia nova vira notificação para todos. Aparelho novo não repete notificações antigas.
- **Batalha agendada e avisos:** no Menu do Dono → Batalha, escolha "agora" ou daqui
  5/15/30 min, 1/2/3 h. A entrada abre 1 minuto antes. Todo mundo no GDPS recebe aviso
  com 1 h, 5 min e 1 min. Na hora, quem entrou tem a fase aberta sozinha.
- **Começo justo:** a fase fica parada no início até todos carregarem (máx. 40 s).
  Depois vem a contagem 5-4-3-2-1 e todos começam juntos do zero.
- **Na fase:** no canto aparece o rival (troca a cada 3 s), com a % dele e a sua posição.
- **Anti-cheat (BattleGuard):**
  - mods proibidos barram a entrada e desclassificam durante a fase;
  - jogo acelerado (speedhack) desclassifica;
  - o servidor só aceita "passei" depois do tempo mínimo da fase, calculado pelo
    tamanho do level (Tiny 5 s, Short 9 s, Medium 27 s, Long 54 s, XL 108 s);
  - prática e start position não contam.
- **Ranking de Candy:** aba Ranking. Conta o Candy juntado no evento (gastar não
  diminui). O Top 1 no fim é o vencedor do evento; o Top 5 aparece em destaque e o
  resto vem paginado.
- **Menu do Dono:** só a conta do Luiz (`owner_account`) e quem ele escolher em
  Jogadores → "Dar painel" / "Tirar painel". Cargo de Owner/Admin no GDPS não dá mais acesso.
- **Publicar fases:** Menu do Dono → **Fases** → "Publicar uma fase minha" → escolher um
  level de Criar > Meus levels.
  - O arquivo vai para `geobr-main-levels/natal-levels/` e a fase entra publicada.
  - Na mesma página, cada fase tem o botão Publicada/Escondida.
  - Fase plataforma entra só no Evento, não na Batalha.
- **Minigames:** removidos. Os 3 mapas gerados saíram do mod e do repositório. As cópias
  no GDPS (IDs 44, 45 e 46, conta do Luiz) estão marcadas como apagadas.

## Servidor

As migrações estão em `supabase/migrations/2026112*natal*.sql` e todas foram aplicadas.
A última é `20261121200000_geobr_natal_launch.sql`.
