# GeoBR V1 RC2 — 1.9.0-beta.2

Android arm64 · GD 2.2081 · Geode 5.9.0 ou compatível.
Atualize o mesmo mod sem apagar os dados.

## Atualização

- Backend original autorizado: migrações social/ranking e avatar/security aplicadas.
- Temporada GeoBR — Expedição ativa de 14/10/2026 a 13/11/2026: 30 etapas, 12 skins nativas em Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing e Jetpack; Tokens, Orbs e estrelas.
- 24 missões publicadas, com objetivos de completar níveis, prática, porcentagem, campanha e plataforma. A sequência reinicia ao terminar.
- Passe com cabeçalho, saldo, barra de XP, cards com prévias de skins, estado de resgate, paginação e missão atual; escala adaptada à altura da tela.
- Mapa com caminho alternado, cards espaçados, progresso e tela nativa de detalhes antes da partida. Fecha o popup antes da transição, retém o nível e bloqueia toques repetidos.
- Main Levels também abre detalhes nativos antes de jogar.
- Botões com tamanho visual limitado ao retângulo disponível; cabeçalho do chat com margens explícitas; feedback separado da caixa de texto.
- Jogadores: ícone e cores vindos do perfil de cada jogador; sincronização de identificador de ícones More Icons. Cada jogador precisa abrir a nova versão para atualizar o registro. Ícones personalizados só são renderizados quando o pacote correspondente está instalado; caso contrário é usado o ícone nativo sincronizado, nunca o ícone personalizado do observador.
- DMs, respostas, notificações dentro do jogo, ranking e cosméticos nativos disponíveis com o backend atualizado.
- Cosméticos públicos no perfil nativo ainda exigem vínculo verificado no ADM; não associamos conta por nome não verificado.
- Mantidas as duas dependências anteriores: GMD API e More Icons.

## Validação real

Build Release Android concluída. ZIP, manifesto, ELF AArch64 e recursos conferidos.
Testes no servidor original com ROLLBACK: missão, XP, Tokens, resgate idempotente, repetição de evento, avatar com slug, privacidade de DMs, bloqueio, notificações e proteção do ranking. Nenhum usuário de teste permaneceu.
Arquivo Loucura Brasileira conferido: XML/GMD com dados gzip válidos, aproximadamente 2.557 objetos. Falta de Supabase não foi confirmada como causa do crash.

Não há GDPS executando neste ambiente. O novo fluxo evita entrada direta em PlayLayer durante um popup, mas a resolução do crash e o acabamento visual precisam ser confirmados no aparelho. Não existe comprovação de compatibilidade com todos os mods instalados.

## Painel ADM

Oito toques rápidos em More Games. A senha antiga está somente como hash no servidor; não foi redefinida nesta atualização. Nenhuma senha ADM está embutida no cliente.

## Dados

Tokens, inventário e XP permanente preservados. A nova temporada começa com progresso próprio. Continua autenticação anônima, sem recuperação após apagar os dados. Eventos de gameplay continuam informados pelo cliente.

As migrações incluídas são histórico de alterações já aplicadas. Não execute a criação de temporada novamente.
