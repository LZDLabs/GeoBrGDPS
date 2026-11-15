# GeoBR RC8 — 1.9.0-beta.8 (Android arm64)

Pacote: `dist/geobr.geobrmod.geode` — instale pelo Geode por cima da RC7. Geode 5.9.0, GD 2.2081. Dependências inalteradas.
SHA-256 do pacote: `cd68bf0d474daab0012d876c9d0578f7a7243e09af4b5b87b6e76605b5a71ab1`
SHA-256 do binário: `af7a09d5b1c1e36b115196f8f37364612d687e97c9584eccea117a8563df28bf`

> Como nas anteriores: nada aqui foi jogado no Geometry Dash (não há jogo/emulador neste ambiente). Compila, linka contra o loader real e passa nos testes automáticos. O roteiro de teste no aparelho está no fim.

> **Antes de instalar:** a migração `20261105180000_geobr_rc8_campaign.sql` precisa estar aplicada (ela libera a versão beta.8 no servidor e lista as fases). Veja "Pendente".

## Correções do último retorno

**God Editor travava ao entrar.** Causa: o menu do God Editor chamava `setHandlerPriority` antes de estar registrado no `CCTouchDispatcher`; o cocos procura o handler, recebe nulo e o desreferencia. Agora a prioridade é definida antes do registro (`setTouchPriority`). Além disso, enquanto o menu de pausa ou um popup estão abertos, a camada do God Editor some e não captura toques (antes ela podia "engolir" o pause), e a animação de fim de fase não dispara se você voar além do fim.

**Área do Hub preta e ícones que não apareciam.** Causa: todo fundo de tema (`visual::theme`) era um `CCClippingNode` com `alphaThreshold`; o shader de alpha-test substitui o do `CCDrawNode` usado como máscara, a máscara não escreve nada e tudo dentro do tema era descartado. Isso deixava preta a página GeoBR (Criar → seta) com os 4 botões invisíveis, e sumia com os ícones dos Arrows na loja e prévias de temas nas recompensas. Os temas agora não usam máscara: a arte é recortada pelo retângulo da textura e as formas são recortadas geometricamente. O mesmo foi removido da demonstração do Arrow no perfil.

**Interface do Arrow na garagem.** A grade nativa continuava visível e os Arrows iam para o canto inferior esquerdo: o `ListButtonBar` do GD fica em (0,0) e posiciona as páginas por dentro, e os campos da garagem não são confiáveis no Android. Agora a aba procura as peças pela árvore de nós (grade por tipo, cursores `GJ_select_001` e pontos de página pelo sprite), esconde, mede a área pela posição real dos ícones na tela e põe os 16 Arrows ali, sobre um painel no estilo GD. A prévia fica onde o GD mostra o ícone. Trocar de aba restaura tudo.

**Física do Arrow em cima de objetos.** Havia um bug real: o Arrow aprende o sinal da velocidade do motor observando o movimento, e isso também rodava no chão e em rampas, onde a colisão empurra contra a velocidade; depois de 3 quadros o sinal invertia e o Arrow quebrava em blocos e rampas. Agora só aprende no ar. Em rampa (inclusive rampas com D block) o solver da Wave do próprio GD mantém o jogador na superfície e ele sobe conforme a física.
Ângulo: **no chão ou em cima de um bloco fica reto, alinhado ao chão** (como a Wave); **na rampa segue a inclinação** (medida pelo movimento real, serve para qualquer rampa); **no ar, ±45°**.
Interpretei o seu "90° em cima do chão" como "alinhado ao chão, em ângulo reto com a gravidade". Se você quis dizer apontando para cima, é uma linha para trocar.

**Crash ao entrar nos Main Levels.** Não consigo reproduzir sem o jogo. A única mudança da RC7 nesse fluxo foi a campanha embutida (arquivos locais + `.gmd` gerados por mim). Esse caminho foi removido: os Main Levels voltam ao fluxo da RC5/RC6 (download do Storage), o mesmo que já abre "Loucura Brasileira". Se ainda crashar, me mande o arquivo mais recente de `geode/crashlogs`.

## Campanha: duas fases NOVAS

As cópias do Deadlocked e do Theory of Everything do RobTop foram descartadas (também do Storage). As duas fases abaixo são **originais**, com layout novo. Quem desenha é `tools/design_campaign.py`, determinístico: gera sempre os mesmos bytes.

| Fase | Música (oficial do GD) | Dificuldade | Trechos |
|---|---|---|---|
| **Teoria Tropical** | Theory of Everything (#11), 83 s, 1x | Hard, 5★ | cubo, nave, cubo, UFO, cubo, bola, **Arrow (9 s)**, nave, cubo, UFO, bola, **Arrow**, cubo |
| **Bloqueio Total** | Deadlocked (#19), 99,5 s, 0.5x–2x | Harder, 7★ | cubo, **Arrow**, nave 2x, bola, UFO, cubo, bola, cubo 2x, **Arrow (7 s)**, nave 2x, UFO, **Arrow 2x**, cubo 0.5x, nave 2x, bola, UFO, cubo |

O RobTop Travel serviu só de referência, para três coisas:
1. **Coordenadas.** No arquivo do GD o chão fica em y=0 e a primeira fileira em y=15. As fases da RC7 usavam 90 e ficavam flutuando 3 blocos acima, por isso os "objetos desalinhados".
2. **Acabamento dos blocos.** O RobTop desenha o contorno só por fora das estruturas, escolhendo uma de 7 peças (ids 1–7, girada ou espelhada) conforme os 4 vizinhos. Medi essa regra nas fases dele e o gerador aplica igual: nenhuma grade interna, tudo na grade de 30.
3. **Sincronia com a música.** As trocas de modo caem nos momentos em que a música muda de seção. No Bloqueio Total, o fundo pulsa nas batidas fortes.

Cada trecho passa num simulador do seu modo antes de entrar na fase. O teste usa 3 variações de física (pulo/gravidade mais fortes e mais fracos) e várias alturas de entrada:
- **Cubo e bola**: busca exaustiva de pulos. Cada padrão (espinhos, degraus, pilares sobre espinhos, pad amarelo, esferas, escadas) só fica se sobrar uma janela de toque de pelo menos 5 quadros na Teoria Tropical e 4 no Bloqueio Total (a 60 FPS).
- **Arrow**: busca exaustiva de toques a 45°, deslizando no chão e no teto, com janela de pelo menos 6 e 5 quadros.
- **Nave e UFO**: um "piloto" que segue o meio do corredor precisa passar com folga em todas as variações.

Detalhes do desenho:
- Os trechos voadores têm entrada livre e funil de saída em rampa.
- A bola sempre devolve a gravidade normal antes do portal seguinte.
- Os trechos de Arrow têm cores marrons, como o portal, e pilares com espinhos que se alternam entre chão e teto, seguidos de um túnel em zigue-zague a 45°.
- Por cima dos trechos de cubo ficam estruturas penduradas no teto, fora do alcance de qualquer pulo, como nas fases do RobTop.

**Supabase**: `main-levels/geobr-teoria-tropical-v1.gmd` e `main-levels/geobr-bloqueio-total-v1.gmd` no bucket `geobr-main-levels`, com download público conferido byte a byte. As cópias do RobTop foram apagadas do bucket. Os arquivos também estão no repositório, em `campaign/`.

## Pendente (precisa da sua confirmação no Supabase)

Para subir e apagar os arquivos usei permissões **temporárias** no Storage, restritas a nomes de arquivo exatos (inserir os 2 novos, apagar os 2 antigos). A migração `supabase/migrations/20261105180000_geobr_rc8_campaign.sql`:
1. remove essas permissões;
2. lista as duas fases na campanha (ordem 2 e 3, depois de "Loucura Brasileira");
3. libera a versão 1.9.0-beta.8.

Ao tentar aplicá-la, o Supabase pediu sua confirmação e ninguém respondeu a tempo. **Até ela ser aplicada, as fases não aparecem e a beta.8 não é reconhecida pelo servidor.**

## Testes executados (resultados reais)

| Teste | Resultado |
|---|---|
| Build Android arm64 Release limpo (NDK r28c, SDK 5.9.0), `--no-undefined` contra o loader real | PASS, 0 warnings |
| `package_v1.py`: ZIP, manifesto 1.9.0-beta.8, ELF AArch64, ícone, 98 recursos idênticos | PASS |
| `static_validation.py`: versão, sem `.gmd` embutido, migração RC8 (fases novas, remoção das 4 permissões temporárias), sem `CCClippingNode`, sem `setHandlerPriority`, 16 skins | PASS |
| `android_symbols.py`: 45 funções do GD exportadas no Android 2.2081 (inclui as 2 novas do God Editor) | PASS |
| `campaign_levels.py`: decodifica como o GD, música oficial, portais do Arrow (2 e 3), chão em y=0, todos os blocos na grade com a peça de contorno certa, reconstrução idêntica revalidando os 30 trechos | PASS |
| Host: ArrowPhysics (268), TextSanitizer (11), Models (20), ShopOrder | PASS |
| Upload no Storage + download público idêntico | PASS |

NÃO executado: GD no aparelho, toque real, visual. As fases foram validadas por simuladores com física aproximada (constantes do GD medidas pela comunidade, testadas numa faixa), não jogadas. A migração RC8 ainda não foi aplicada (veja acima).

## Limitações reais

1. A garagem depende de achar o `ListButtonBar` e os sprites `GJ_select_001`/`gj_navDotBtn_*`. Se não achar, os Arrows vão para a posição padrão do GD, sobre um painel opaco.
2. As fases usam o estilo clássico do RobTop (blocos com contorno, espinhos, decoração de chão 18–21) e cores por trecho. Não há decoração 2.0 (brilhos, partículas), porque não consigo conferir o visual sem o jogo.
3. A física de nave e UFO é aproximada (o GD não publica as constantes). Por isso esses trechos são largos e testados numa faixa de valores. Ajuste fino de ritmo e decoração deve ser feito no editor depois de jogar.
4. Push para o GitHub continua bloqueado (403).

## Checklist no aparelho

1. Criar → seta: página GeoBR com fundo azul e os 4 botões visíveis.
2. Loja > Arrows: ícones visíveis nos cartões; recompensas de tema com prévia.
3. Garagem → aba Arrow: grade nativa some, 16 Arrows no lugar, cadeado nas bloqueadas, voltar ao cubo restaura.
4. Arrow: em cima de bloco (reto), descendo/subindo rampa com D block (segue a rampa), no ar (±45°), com gravidade invertida.
5. Main Levels: lista abre; Teoria Tropical e Bloqueio Total baixam e jogam com a música oficial; portal marrom nos trechos do Arrow; dá para completar cada trecho.
6. Fase sua → pausa → God: abre sem travar; pausar com o God aberto e voltar; Salvar; Sair.
