# GeoBR V1 RC5 — Android arm64

Versao: 1.9.0-beta.5. Geometry Dash 2.2081, Geode 5.9.0 ou compativel.
Instale o .geode pelo gerenciador Geode, substituindo a versao anterior do mesmo mod.
Nao apague os dados do mod: eles guardam a sessao e o progresso pendente.
Dependencias mantidas: GMD API 1.5.0 e More Icons 2.1.3. Sem novos mods.

## Alteracoes

- Botoes aprovados, com texto aplicado a partir do arquivo real PUSAB___.otf.
- Seis novos baus, do comum ao mitico, com 4/5/6/7/8/10 itens.
- Abertura: um item por vez, Proximo, Revelar tudo, resumo final. Os itens sao concedidos uma unica vez pelo servidor antes da animacao.
- Historico: GeoBR > Opcoes > Historico. Ultimas 30 caixas abertas, toque para rever, inclusive depois de fechar uma abertura. Nao concede os itens novamente.
- Temas Fogo, Aurora e Cosmos a partir das artes aprovadas. Fundo preso ao painel nativo e recortado no interior; sem triangulos saindo da moldura. A arte usa preenchimento proporcional e recorte, sem deformar desenhos. Outros estilos anteriores continuam disponiveis.
- Loja: Experimentar tema antes de equipar e filtro Colecao (somente os itens possuidos).
- TOP: em detalhes ocupa a antiga linha das estrelas, com as estrelas e moedas deslocadas para baixo. Em listas usa espaco separado acima de View; a coluna de dificuldade permanece nativa.
- Inicio GeoBR com atalhos para Passe, Levels, Baus, DMs, Jogadores e Opcoes.
- Chat: avatar do autor, mensagem, citacao de resposta e horario, com cancelar resposta.
- DMs: bolhas distintas para voce e interlocutor, citacao, avatares, horario e contagem de conversas nao lidas. Mensagens extensas quebram linha.
- Painel ADM com navegacao lateral e area de registros separada. Seguranca abre Jogadores, Punicoes, Versoes e Historico.
- Missao mostra Enviando, Aguardando sincronizacao, Sem conexao (reenvio salvo) ou Confirmado, conforme o estado real.
- Opcoes de interface Compacta e Media, com previa imediata na propria janela e aplicacao nas demais ao reabrir.
- Mapa permanece Em breve.

## Moderacao

Abra More Games com 8 toques rapidos e use sua senha atual (nao foi alterada).
Em Seguranca > Jogadores escolha o nome/UUID correto:
- Mute: prazo obrigatorio em minutos; bloqueia novos envios no chat geral e nas DMs.
- Ban do chat: bloqueia novos envios no geral, mantendo DMs e progressao.
- Ban do mod: suspende os recursos GeoBR e as operacoes de progressao no servidor.
- Prazo zero significa permanente para bans; para mute nao e permitido.
Motivo obrigatorio. A aplicacao pede revisao; revogue em Punicoes.
As acoes sao registradas, sem expor registros privados aos jogadores.
Nao apliquei punicoes em nenhum jogador real.

Em Seguranca > Versoes selecione ou adicione uma versao, escolha LIBERADA/BLOQUEADA e escreva a mensagem de atualizacao. A versao atual e 1.9.0-beta.5; a anterior e 1.9.0-beta.4. Ambas seguem liberadas nesta entrega. Clientes antigos sem cabecalho de versao sao tratados como beta.4.
A RC5 consulta o estado a cada 10 segundos. Em versoes anteriores o bloqueio aparece nas operacoes de servidor, pois elas nao possuem essa tela de aviso. Alteracoes nao encerram uma fase do GD em andamento.
O bloqueio amplo de leitura de catalogos nao foi aplicado: a revisao automatica o rejeitou pelo impacto sobre acessos existentes. O controle entregue bloqueia as telas RC5 e as operacoes protegidas de progressao/social; catalogos publicos continuam publicos.

Os bloqueios sao de recursos do GeoBR: nao desinstalam o mod, nao encerram o GD e nao representam protecao contra cliente adulterado. A identidade atual e a sessao persistida da instalacao; apagar os dados/criar outra sessao pode permitir evasao. Para ban permanente resistente a reinstalacao, e necessario vinculo comprovado da conta GDPS no login. Nao e correto prometer ban por aparelho com a autenticacao anonima atual.
As notificacoes sao dentro do jogo; sem push Android com o aplicativo fechado.

## Validacao

- Compilacao real Android64, ELF AArch64 e pacote ZIP integro.
- SQL live_rc5: mute, prazo, ban chat/DM, ban mod, leitura privada negada, revogacao, versao desabilitada, trilha ADM e novas DMs/lidas.
- Regressoes: live_rc4, live_progression_v3 e live_v1_regression (fixtures isoladas e ROLLBACK).
- Recursos, limites de layout declarado e contratos da fila de missoes verificados por scripts.
- Revisao Supabase: sem ERROR; tabelas privadas intencionalmente sem politicas de acesso direto. Avisos de sessoes anonimas sao inerentes ao modelo existente; o ADM exige sessao autenticada de painel.

Nao ha jogo/emulador nesta maquina. Os testes de layout numerico e compilacao nao substituem validacao visual ou de crash no aparelho. Esta e uma RC para essa validacao, e nao uma afirmacao de que toda combinacao de 38 mods foi testada.

### Checklist no aparelho

1. Nuisance TOP 1 nos detalhes, lista normal e lista compacta: texto legivel, estrelas e moedas sem colisao.
2. Trocar Fogo/Aurora/Cosmos, reabrir o proprio perfil e um perfil remoto, rolar comentarios: fundo alinhado dentro da borda.
3. Abrir caixa e usar Proximo ate o resumo; fechar no meio e rever em Historico; saldo nao duplica.
4. Chat e DMs com mensagem longa, citacao, teclado Android, avatar de outro jogador e novas mensagens.
5. Missao com conexao normal e temporariamente sem internet: premio apenas apos confirmacao.
6. ADM: mute de teste de 1 minuto, ban/revogacao e versao ficticia bloqueada. Nao bloquear beta.5 durante validacao geral.
7. Alternar Compacta/Media e verificar controles em tela pequena.

## Fonte

src/: cliente C++.
database/rc5-moderation.sql e rc5-social.sql: SQL final das novas funcoes e tabelas, ja aplicado no projeto GeoBR. Nao reaplique o primeiro arquivo sem adaptar as operacoes de rename/trigger.
tests/: testes reais e verificacoes de empacotamento.
resources/: arquivos usados pelo mod.
art-source/: pranchas aprovadas e fonte enviada para proveniencia e novos recortes.
tools/prepare_assets.py: extracao dos sprites das pranchas incluidas; requer Pillow.

As artes de conceito foram geradas e aprovadas pelo usuario; a tipografia dos botoes foi aplicada diretamente com a fonte enviada. Nao sao apresentadas como desenhos manuais.
