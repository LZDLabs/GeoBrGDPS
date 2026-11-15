# GeoBR V1 RC6 — Android arm64

Versão **1.9.0-beta.6**. Geometry Dash 2.2081, Geode 5.9.0. Base obrigatória: código-fonte da RC5 (commit `a3ee6a2` deste repositório, importado sem alterações).
Pacote: `dist/geobr.geobrmod.geode` — instale pelo Geode substituindo a RC5. Não apague os dados do mod (sessão e progresso pendente).
Dependências inalteradas: GMD API 1.5.0 e More Icons ≥ 2.1.0. Nenhum mod novo.

SHA-256 do pacote: `85fa08e0a0688dcb124f071f7518a185ef110e6189afd47e1159d1c94f1a5020`
SHA-256 do binário `geobr.geobrmod.android64.so`: `a24e5ed12590d5142ed79f4b2af807d554c9d5a6581610cc02129f61c6a81a22`

> Leia a seção **Limitações reais** antes de publicar. Nada aqui foi executado dentro do Geometry Dash: não há jogo nem emulador neste ambiente.

---

## 1. Gamemode Arrow

### Mecânica (implementada em `src/arrow/`)

| Regra | Implementação |
|---|---|
| Diagonal de 45° | `ArrowMotor::velocity` (`ArrowPhysics.hpp`) define a velocidade vertical com módulo igual ao da diagonal normal do motor do GD. |
| Cada toque inverte | `pushButton` é consumido pelo Arrow e inverte a direção; `releaseButton` nunca move o Arrow. |
| 45° também no mini | Durante `updateJump` o tamanho é tratado como normal; o mini mantém 45° (a Wave mini fica ~63°). Hitbox e visual continuam mini. |
| Gravidade | "Subir" significa "afastar-se do chão": portais de gravidade invertem o sentido na tela, como esperado. |
| Entrada pelo portal | Se o dedo estiver pressionado ao passar no portal, o Arrow começa subindo; senão, descendo. |
| Saída | Qualquer outro portal de gamemode (inclusive a Wave comum) desliga o Arrow e devolve os sprites nativos. |
| Checkpoints / Practice | `PlayLayer::createCheckpoint` grava `{ativo, direção}` de cada jogador; `loadFromCheckpoint` restaura (e reaplica no quadro seguinte). |
| Restart | `resetLevel` reaplica o modo inicial quando não houve checkpoint. |
| Dual | O portal é repassado aos dois jogadores; o segundo jogador criado pelo portal Dual copia o estado do primeiro; cada jogador tem estado próprio (2 jogadores funciona). |
| Sincronização de input | O estado "segurando" do motor é espelhado a partir do Arrow por chamadas reais a `pushButton/releaseButton`, com proteção de reentrada. |

**Colisão:** o Arrow roda sobre o solver de colisão do *dart* do próprio GD (mesma hitbox pequena, desliza em chão/teto, morre em lateral, slopes nativos). Isso é intencional: reescrever a colisão sólida/hazard do GD fora do motor quebraria slopes, Dual, triggers de colisão e compatibilidade com outros mods. O que é próprio do Arrow — entrada, direção, ângulo fixo de 45° em qualquer tamanho, regras de portal/checkpoint/gravidade, visual e efeitos — está em código próprio e testado em host (`tests/ArrowPhysicsTests.cpp`, 268 verificações). Proposta para uma RC7, se quiser hitbox própria: substituir `getObjectRect` do jogador quando o Arrow estiver ativo, depois de validar no aparelho que as regras atuais estão certas.

### Visual e efeitos (criados do zero)

- `tools/generate_rc6_art.py` gera toda a arte por geometria (reprodutível): 4 skins × 4 camadas (`primary` = Col 1, `secondary` = Col 2, `glow` = Glow, `detail` sem tinta), portal marrom (camadas de trás e da frente), faísca e textura de rastro.
- Ícone aponta na direção real do movimento (achata ao deslizar), "pop" a cada toque, faíscas na cor Col 2, explosão âmbar ao entrar pelo portal, rastro próprio (CCMotionStreak) na cor Col 1. O rastro e os sprites nativos da Wave ficam ocultos enquanto o Arrow está ativo.
- Glow: igual ao GD (padrão), sempre ligado ou desligado — configurável no popup do Arrow. A cor do Glow segue a cor de glow do icon kit.

Skins: **Flecha** (ponta larga, cauda entalhada), **Dardo** (losango com núcleo Col 2), **Pena** (haste fina com aletas Col 2), **Cometa** (chevron duplo).

### Portal marrom

O portal é gravado como o portal Wave nativo (ID 660) com a flag nativa **Hide** e **Z Order 97** como marcador. São propriedades normais do GD, então o marcador sobrevive a salvar, copiar/colar, desfazer e upload sem chaves customizadas. Por cima, o GeoBR desenha o portal marrom (parte de trás atrás do jogador, frente na frente, pulso âmbar animado; verde quando selecionado no editor).
Sem o GeoBR, o mesmo objeto funciona como um portal Wave invisível — o level continua jogável.

### Editor

- Botão do portal marrom inserido logo após o portal Wave na aba de portais (sobrevive a reconstruções do menu). Tocar nele e colocar objetos cria portais Arrow.
- Configurações do level: botão **ARROW** ao lado do seletor de gamemode inicial. Ativo, grava `kGBA,1` no cabeçalho do level e mantém `kA2=4` (Wave) como fallback do GD vanilla. Escolher outro gamemode no seletor nativo desliga o Arrow.
- Funciona no teste do editor (`onPlaytest`) e na execução normal (`setupLevelStart`/`resetLevel`).

### Perfil

- Botão com o Arrow (balançando entre ±45°) logo após a fileira de gamemodes do `ProfilePage`.
- Próprio perfil: popup com demonstração animada (toque inverte), as 4 skins selecionáveis, canais Col 1/Col 2/Glow e modo de Glow. A escolha é salva localmente e sincronizada (`geobr_profile_arrow`).
- Perfil de outro jogador: mostra o Arrow com as cores dele (Col 1/Col 2/Glow do GJUserScore) e a skin vinda do servidor, somente leitura.
- Também acessível em **GeoBR > Arrow**.

## 2. Correções da auditoria (cliente)

| Problema na RC5 | Efeito | Correção |
|---|---|---|
| `AdminSafetyPopup`: `get(...).unwrap()` em "restrictions"/"log"/"revoked_at" | Fechamento do jogo se a resposta não tivesse a chave | leitura opcional com fallback |
| `CommunityTab`: `std::stoll` no id de resposta | Exceção não tratada → fechamento | `std::from_chars` validado |
| `SafetyForm`: `std::stoi` com try/catch | Dependia de exceções | `from_chars` |
| `rewardIcon`: `stringOr(...,"item_id").substr(6)` avaliado sempre | `std::out_of_range` com ids curtos | `ui::slice` seguro |
| DMs: id de envio retido após falha de rede travava "Voltar"/troca de conversa | Usuário preso na conversa | id reaproveitado só para o mesmo destinatário+texto; navegação liberada |
| TextSanitizer aceitava bytes de continuação soltos, C0/C1, F5+ | UTF-8 inválido enviado ao servidor | validação estrita (5 novos testes) |
| Toque no próprio avatar no chat abria DM consigo mesmo | Erro do servidor | bloqueado com aviso |
| Chat rolava ao topo a cada atualização de 3 s | Leitura interrompida | mantém a posição se o leitor não estiver no topo |
| `create()->show()` sem checagem (Quest/TierShop) | Ponteiro nulo | checagem |
| Polling: 4 requisições a cada 10 s, inclusive durante fases | Bateria/dados/carga | acesso a cada 20 s e pausado em fases; estado a cada 30 s (imediato se houver fila); avisos a cada 60 s; social mantido em 10 s |
| Catálogo de Main Levels redesenhava o card a cada 20 s | Animações reiniciando | só redesenha se o catálogo mudou |
| Opções > Coleção abria a loja sem filtro | Inconsistência | abre já filtrada |
| `GeoMapPopup.hpp` com membros declarados e nunca definidos | Código morto | removido |

## 3. Supabase (migração `20261031120000_geobr_rc6_arrow_identity`, aplicada)

- **Segurança:** o trigger do chat confiava no `username` enviado pelo cliente — qualquer jogador podia postar como "ADM GeoBR". Agora o nome vem do perfil do remetente.
- `geobr_profiles.arrow_skin` (1–4) e `arrow_glow`; RPC `geobr_profile_arrow` (privada `security definer` + wrapper `security invoker`, sem acesso `anon`, exige acesso liberado).
- Linha de versão `1.9.0-beta.6` liberada; beta.4 e beta.5 continuam liberadas.
- 10 índices para as chaves estrangeiras apontadas pelo advisor de performance.
- Migração apenas para frente: nenhuma migration antiga foi reaplicada. Nenhuma chave privada ou `service_role` foi usada no cliente (a chave do mod.json é a publishable).

## 4. Interface

- Design system em `src/ui/UIHelpers`: moldura quase reta (chanfro de 2,5 pt), contorno preto, borda branca fina, sombra deslocada e faixa de profundidade; botões em Pusab (`bigFont`) com sombra; botões desabilitados ficam esmaecidos (o item nativo do GD não fazia isso).
- Aplicado a **todas** as janelas (18 popups) pela troca do fundo nativo arredondado e pelos helpers compartilhados de painel/botão; a tela Início foi reorganizada (faixa do jogador com nível e Tokens, abas, 7 atalhos incluindo Arrow).
- **Temas de perfil:** os 10 temas agora têm arte própria. Na RC5, Eclipse Rubro, Cristal Astral e Nebulosa Real reaproveitavam as artes de Fogo, Aurora e Cosmos; Gelo, Floresta, Neon e Ouro eram desenhos simples. Fogo, Aurora e Cosmos mantêm a arte aprovada.
- As demais telas mantêm o layout aprovado da RC5 com o novo acabamento; não foram reposicionadas elemento a elemento.

## 5. Testes executados (resultados reais)

| Teste | Resultado |
|---|---|
| Build Android arm64 Release (NDK r28c / Clang 19.0.1, Geode SDK 5.9.0, bindings `7f6c2a75`), link com `--no-undefined` contra o loader Geode compilado do código-fonte v5.9.0 | PASS, 0 warnings |
| Linha de base: RC5 sem alterações no mesmo toolchain + `package_v1.py` da RC5 | PASS |
| `tests/package_v1.py` (RC6): ZIP íntegro, manifesto, ELF AArch64, 49 recursos idênticos byte a byte | PASS |
| `tests/static_validation.py`: manifesto, recursos, 10 temas, 4 skins × 4 camadas, unidades de compilação, sem senha ADM ou service_role | PASS |
| `tests/android_symbols.py`: 27 funções do GD hookeadas/chamadas pelo Arrow exportadas no `libcocos2dcpp.so` 2.2081 | PASS |
| `tests/ArrowPhysicsTests.cpp` (host) | PASS — 268 verificações |
| `tests/TextSanitizerTests.cpp` (host) | PASS — 11 testes |
| `tests/ModelParsingTests.cpp` (host) | PASS — 20 validações |
| `tests/shop_order_test.cpp` (host) | PASS |
| Ensaio da migração com ROLLBACK | PASS |
| `tests/live_rc6.sql` no banco real (ROLLBACK) | PASS |
| `tests/live_rc5.sql` (regressão) no schema RC6, com cabeçalho beta.6 | PASS |
| Advisor de segurança Supabase | 0 ERROR; mesmos avisos herdados (auth anônima, tabelas privadas sem policy, proteção de senha vazada) |

**NÃO executado:** Geometry Dash em aparelho/emulador, toques reais, render visual, combinação com outros mods. Ver checklist.

## 6. Limitações reais

1. **Sem validação dentro do jogo.** Compilar e linkar provam que os símbolos existem, não que o comportamento está certo. Pontos que dependem de detalhes internos do GD e precisam de teste no aparelho:
   - sinal da velocidade vertical em gravidade invertida (há aprendizado automático com votação, mas o padrão é "espaço de tela");
   - ordem em que o GD chama `playerWillSwitchMode`/`toggleDartMode` ao tocar o portal (há dois caminhos de detecção);
   - posição do botão ARROW nas configurações do level e do botão na fileira de gamemodes do perfil (heurísticas de posição);
   - camadas do overlay do portal (atrás/na frente do jogador).
2. **Start pos:** o Arrow como gamemode de start pos depende de o GD serializar as configurações do start pos por `LevelSettingsObject::getSaveString/objectFromDict`. Não verificado.
3. **Hitbox:** igual à do dart (ver seção 1).
4. **Orbs/pads:** o Arrow os trata como a Wave (ignora), porque roda sobre o modo dart.
5. **GD vanilla:** portal Arrow = portal Wave invisível; level com início Arrow começa como Wave.
6. **Ferramentas:** o build do Geode CLI foi bloqueado neste ambiente; o `.geode` foi montado por `tools/geode_pack.py`, que reproduz o layout do CLI 3.9.0 para recursos `files` (confirmado pelo teste byte a byte). A GMD API usada só para linkar foi compilada do código-fonte 1.5.0 (com a versão do Geode ajustada localmente); no aparelho vale a GMD API instalada.
7. **GitHub:** o push para `idepth26-source/continua-o-Radio` foi recusado (403, app sem acesso). O código está commitado localmente na branch `claude/geobr-rc6-arrow-o90d49`.
8. Sessão anônima, ban por reinstalação e notificações com app fechado: mesmas limitações documentadas na RC5.

## 7. Checklist no aparelho

1. Level com portal marrom: entrar, tocar várias vezes (normal e mini), sair por portal de cubo/nave/Wave comum.
2. Portal de gravidade com o Arrow ativo: o sentido na tela inverte e os toques continuam alternando.
3. Practice: checkpoint subindo e outro descendo; morrer e conferir a direção restaurada. Restart normal.
4. Dual: portal Dual com o Arrow ativo; modo 2 jogadores.
5. Editor: aba de portais → portal marrom; salvar, sair, reabrir, copiar/colar, desfazer; testar o level. Configurações → ARROW; testar e jogar normalmente; trocar o gamemode no seletor nativo.
6. Perfil próprio e de outro jogador: botão do Arrow, troca das 4 skins, Glow em GD/Ligado/Desligado com cores diferentes no icon kit.
7. Interface Compacta/Média em tela pequena: Início, chat (mensagem longa, citação), DMs (falha de rede no envio e depois Voltar), loja, GeoPass, baús, ADM > Segurança.
8. Temas: trocar entre os 10 e abrir o próprio perfil e um remoto.

## 8. Reprodução do build

```
source /opt/tc/env.sh            # NDK, GEODE_SDK, deps e PATH do CLI offline
tools/build_android64.sh . build-rc6
python3 tests/package_v1.py build-rc6/geobr.geobrmod.geode
```
Detalhes do toolchain em `BUILD-RC6.md`.
