# GeoBR Mod — Beta 1.8

Geometry Dash 2.2081 / Geode 5.9.0. Pacote compilado para Android 64 bits.

## Correções desta versão

- Corrigida a compra bloqueada por um falso pedido pendente: objetos vazios da Beta 1.7 são reconhecidos como estado inativo. Pedidos reais mantêm seu identificador em tentativas de reconexão.
- Textura própria para os botões de ação, inclusive os bloqueados/resgatados do passe; removida a dependência do sprite de círculo usado nos números das etapas.
- Tier Shop paginada com duas prévias grandes, confirmação de preço e saldo restante, compra de temas/títulos/skips e opção Equipar para cosméticos adquiridos.
- Seis temas ilustrados: Fogo, Gelo, Floresta, Cosmos, Neon e Ouro. Os mesmos itens e preços foram mantidos.
- **Perfil → Jogadores** ou **Comunidade → Jogadores** abre a busca por nome. Toque no cartão para ver o ícone, tema equipado, título e XP público.

## Instalação e navegação

1. Importe `GeoBR-Mod-Beta-1.8-Android64.geode` no Geode e reinicie o jogo.
2. Mantenha GMD API 1.5.0 e More Icons 2.1.x instalados.
3. No início, **GeoBR** abre somente **Avisos** e **Comunidade**.
4. Na tela de Create, Daily, Weekly etc., use a **seta direita** para abrir a seção GeoBR. Os quatro botões com texturas próprias são **Main Levels**, **GeoPass**, **Tier Shop** e **Perfil**. A seta de voltar restaura a tela original.

Arrow e Collab continuam removidos. A campanha, notícias, comunidade e os três Street Robots foram preservados. O ZIP contém o código-fonte, as texturas e a documentação; o `.geode` é o arquivo instalável.

## GeoPass: um caminho

A temporada inicial tem **30 etapas**, em uma única fileira horizontal, com quatro recompensas por página. As setas percorrem a fileira. O cabeçalho mostra a etapa atual, o XP acumulado, o limite da próxima etapa e quanto falta. A barra indica o avanço até ela.

Complete a quest atual para ganhar automaticamente XP e Tokens. Ao liberar uma etapa, toque **Resgatar** no prêmio. Tokens, Orbs, estrelas e skins nativas do GD continuam disponíveis. O passe não possui trilha premium separada.

## Quest Pass: uma por vez

Há **64 variações**, organizadas em sequência. Somente a quest atual recebe progresso; ao terminá-la, a próxima entra automaticamente. A sequência volta ao início após a última quest, em uma nova rodada.

Inclui completar dois Autos diferentes; morrer exatamente em 46%; chegar a porcentagens; morrer em várias tentativas; concluir dificuldades Easy a Demon; jogar no normal ou prática; níveis clássicos e de plataforma; campanha GeoBR, níveis oficiais e níveis online.

- Quando o objetivo pede mais de um level, os levels precisam ser diferentes naquela quest.
- Objetivos de morte contam tentativas diferentes. O exemplo de 46% usa a porcentagem inteira exibida pelo GD, somente no clássico e modo normal.
- Testes do editor e cópias locais não publicadas não contam. Os Main Levels GeoBR são reconhecidos separadamente.
- Uma tentativa pertence à quest que estava ativa quando começou. Ela não concede progresso à quest seguinte retroativamente.
- Eventos repetidos por falha de conexão não repetem a recompensa.
- Não há lista de missões paralelas nem renovação diária do antigo sistema. O cartão inferior do passe mostra a quest atual, sua barra e seus prêmios. **Ver Quest** abre os detalhes.

## Tier Shop e perfil

A loja usa Tokens, mostra o preço e o saldo restante antes da confirmação. Depois da compra de um tema ou título, toque **Equipar** no mesmo cartão. Skips são aplicados imediatamente. **Atualizar** retoma uma compra que ficou sem confirmação por falha de conexão, usando o mesmo pedido.

| Categoria | Itens iniciais |
| --- | --- |
| Temas | Fogo, Gelo, Floresta, Cosmos, Neon, Ouro |
| Títulos | Iniciante, Antigao, Rico, Explorador, Persistente, Colecionador, Veterano, Lendario, Mestre do Dash, Estrela GeoBR |
| Skip de etapa | Avança até o próximo limite de XP do GeoPass; os prêmios continuam sendo resgatados pelo jogador |
| Skip de quest | Troca a quest atual sem conceder o XP ou os Tokens dela |

Temas e títulos aparecem no **Perfil GeoBR**. Use **Jogadores** nesse perfil ou na Comunidade para buscar outras pessoas que já abriram o mod; os resultados vêm dos perfis GeoBR, não de uma varredura das contas do GDPS. Toque no nome de uma pessoa na Comunidade para abrir seu perfil GeoBR. No próprio perfil nativo do GD, o tema também colore o nome e sua faixa. A identidade GeoBR não é vinculada de forma verificada à conta oficial do GD; por isso o mod não aplica o seu tema a perfis nativos de outras pessoas.

O nível pessoal usa XP permanente ganho pelas quests e mostra sua própria barra. A fórmula é `nível = piso(raiz(XP total / 100)) + 1`: nível 2 em 100 XP, nível 3 em 400, nível 4 em 900. Virar a temporada não zera esse XP. Comprar skip de etapa não aumenta o nível pessoal.

## Painel ADM

Toque **oito vezes rapidamente no botão nativo More Games**, no menu inicial, com menos de 0,8 segundo entre os toques. Digite a senha já configurada. O clique normal em More Games é executado após essa pequena janela. A engrenagem extra foi removida.

A sessão ADM dura vinte minutos e termina ao fechar o painel. Há abas de Notícias, Avisos globais, Temporadas, Prêmios, Quests, Main Levels e Tier Shop. Após salvar um formulário, volte e atualize a lista.

- Quests: configure ordem, tipo (`complete`, `death`, `reach`), quantidade, dificuldade, origem, modo, formato, faixa percentual, XP, Tokens e publicação. Para morrer em 46%, use `death`, mínimo 46, máximo 46 e `classic`.
- Uma quest já atribuída guarda sua definição até ser concluída ou pulada. Editar o catálogo altera as próximas atribuições, sem reescrever progresso em andamento.
- Novas temporadas recebem uma cópia editável das quests e dos prêmios da temporada mais recente. Datas publicadas não podem se sobrepor.
- Tier Shop: edite preços, títulos, descrição e publicação; novos títulos podem ser cadastrados sem atualizar o mod. Temas usam um dos seis estilos gráficos incluídos. Itens já comprados devem ser despublicados em vez de apagados, para preservar o inventário.
- Main Levels: escolha um `.gmd`, `.gmd2` ou `.lvl` válido do GDShare, até 12 MB, configure dificuldade, estrelas, moedas, música e publique. As mudanças são globais; arquivos vazios são recusados.

## Conexão e dados

A sessão Supabase é persistida nesta instalação. Tokens, compras, inventário, quests e XP são calculados pelo servidor. Orbs, estrelas e desbloqueios de skins são entregues ao save local do GD com recibos. Skins já possuídas não têm compensação automática.

Não apague os dados do Geode: esta beta usa autenticação anônima, sem recuperação de conta ou vinculação verificada ao login oficial do GD. Uma compra sem resposta mantém o mesmo identificador ao tocar **Atualizar**, evitando nova cobrança. Quest offline usa a última definição recebida; eventos de uma quest ou temporada já encerrada não avançam a nova.

## Compilação e verificação

Requisitos: Geode CLI 3.9.0, SDK 5.9.0, CMake 3.31.6, Android NDK r28c, C++23. Defina `GEODE_SDK` e um perfil Android64.

```sh
geode sdk install-binaries -p android64 -v v5.9.0
geode build -p android64 --ndk /caminho/android-ndk-r28c --config Release
```

As migrações 1.7 já foram aplicadas ao projeto GeoBR. Veja `SUPABASE_SETUP.md`. Os testes de servidor estão em `tests/backend-v17.sql`; executam em transação revertida. Não há jogo/aparelho em execução neste ambiente: a validação no GD real permanece pendente, conforme `BETA_TESTING.md`.
