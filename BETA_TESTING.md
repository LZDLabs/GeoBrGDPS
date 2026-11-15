# Roteiro em aparelho — Beta 1.8

A compilação e os testes da API não executam a interface dentro do Geometry Dash. Validar no GDPS com Geode 5.9.0 / GD 2.2081:

1. Atualizar a versão anterior sem apagar os dados do mod. Abrir Tier Shop; não deve aparecer falso pedido pendente.
2. No GeoPass, percorrer as páginas. Botões Resgatar/Bloqueado/Recebido e números de etapa precisam aparecer nas qualidades baixa, média e alta.
3. Comprar um tema com saldo suficiente. Confirmar o débito único, tocar Equipar, fechar a loja e ver o tema no perfil.
4. Tentar um item mais caro que o saldo: informar quanto falta e não descontar Tokens.
5. Comprar título, skip de quest e skip de etapa. Só o skip de etapa avança o passe; nenhum skip concede XP pessoal.
6. Interromper a rede após confirmar uma compra e usar Atualizar ao voltar: receber o resultado sem nova cobrança. Reabrir o jogo também deve manter o mesmo pedido.
7. Perfil → Jogadores: buscar o próprio nome. Com outro jogador usando a versão nova, buscar o nome dele e conferir ícone, tema, título e nível. Toque no nome no chat também deve abrir o mesmo perfil GeoBR.
8. Conferir botões e teclado em telas estreitas, sem sobreposição; conferir as seis prévias da loja.
9. Abrir Main Levels, Avisos e Comunidade e voltar. Conferir que a página GeoBR e More Games continuam com o comportamento anterior.

## Histórico de validação 1.7

# Verificação no aparelho — Beta 1.7

Estes testes de execução no Geometry Dash ainda precisam ser feitos. Compilar não comprova ausência de crashes.

1. Atualizar para 1.7, reiniciar e abrir GeoBR no início: somente Avisos e Comunidade. Não deve existir engrenagem extra.
2. Na tela Creator/Daily, tocar a seta lateral: quatro cartões GeoBR com texturas distintas. Voltar deve restaurar a tela original. Conferir em 16:9 e na tela larga do aparelho, com os outros mods instalados.
3. Abrir GeoPass: uma fileira de quatro prêmios por página, setas, XP, quanto falta para a próxima etapa e somente uma quest atual. Conferir nomes longos e etapa 30.
4. Completar dois Autos diferentes no normal. A quest deve avançar e dar 125 XP/8 Tokens uma vez. Repetir o mesmo Auto não completa os dois objetivos distintos.
5. Morrer em 45% não conclui a segunda quest. Morrer em 46% no clássico/normal deve avançar. Prática não conta para essa quest.
6. Conferir objetivos de prática, plataforma, chegar a porcentagem, níveis oficiais, online e Main Levels; testes do editor não contam.
7. Comprar tema e equipar. Abrir Perfil GeoBR, próprio perfil nativo e seu nome na Comunidade. Conferir temas e título sem cobrir estatísticas.
8. Comprar skip de quest: cobrar uma vez e trocar objetivo, sem seu XP/Tokens. Comprar skip de etapa: avançar o limite do passe sem mudar XP permanente. Resgatar prêmio nativo e conferir o save após reiniciar.
9. Fechar a loja enquanto uma compra está em andamento e reabrir. Desligar a rede durante resposta e tocar Atualizar após reconectar. Nunca cobrar o mesmo pedido duas vezes.
10. Oito cliques rápidos no More Games abrem o pedido de senha. Um clique comum deve preservar sua ação após 0,8 segundo. Senha errada é recusada; a correta abre ADM. Fechar e reabrir popups durante requisições sem crash.
11. No ADM, conferir formulários de Quests/Tier Shop e salvar como rascunho. Notícias, avisos e Main Levels continuam globais. Não publicar dados de teste sem querer.
12. Abrir editor e pausa: ausência dos antigos módulos Arrow/Collab. Testar transições normais, mini, dual e prática do GD com coleta de quests ativa.

Reportar versão do mod/GD/Geode, arquitetura, lista dos outros mods, passos e crash log quando houver.
