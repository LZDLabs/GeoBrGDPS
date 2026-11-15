# Build e verificações — GeoBR 1.8.0-beta.1

Compilação concluída em 2026-09-09: **Android arm64-v8a, Release, Geometry Dash 2.2081, Geode 5.9.0**.

- Geode CLI 3.9.0; CMake 3.31.6; Android NDK r28c, Clang 19.0.1.
- SDK: `f326379a1a9ca90201903e009137e3da1aa4c375`.
- Bindings: `7f6c2a75742856de88dad354e576dcff8a28e881`.
- Dependências resolvidas nesta compilação: GMD API 1.5.0 e More Icons 2.1.3.
- Resultado: `[100%] Built target GeoBRMod_PACKAGE`.
- Pacote: `12520545` bytes, ZIP íntegro, manifesto 1.8.0-beta.1, único ELF Android 64 bits / AArch64.
- SHA-256: `cfdf440d43d9926ca6ab942c28611fc8c2b6591db93cd0616b4afff55bd47ee5`.
- Sete novas imagens conferidas byte a byte dentro do pacote: seis temas e textura própria de botão. As texturas anteriores, Token e robots também estão presentes.

## Causa corrigida

`matjson::Value()` constrói um objeto vazio; ele não representa JSON null. A Beta 1.7 testava somente `isObject()` para detectar compra pendente e bloqueava toda compra nova quando não havia pedido.

Agora a presença de um pedido exige dono, argumentos e identidade de compra. O estado inativo usa `matjson::Value(nullptr)`. Campos opcionais da RPC usam null. O UUID de pedidos reais é preservado em reconexões e após reabrir a loja. Código de referência da dependência: [matjson 3.3.1, value.cpp](https://github.com/geode-sdk/json/blob/v3.3.1/src/value.cpp).

## Verificações executadas nesta versão

- `tests/shop_order_test.cpp`, compilado e executado em host com assertivas habilitadas: reproduz o objeto vazio legado, confere null, persistência/recuperação da identidade de pedido, snapshot para skips e limpeza após confirmação. O JSON produzido por esse teste é enviado sem mudanças na primeira compra HTTP.
- `tests/http_shop_profiles.py`, pela API real do Supabase com chave pública e uma sessão de teste autenticada: compra de tema, repetição sem nova cobrança, equipar tema, comprar/equipar título, skip de quest, skip de etapa sem XP pessoal, rejeição por saldo insuficiente, bloqueio de alteração direta da carteira, sincronização do avatar, busca por nome e leitura pública de cosméticos.
- `tests/backend-v18.sql`, executado com ROLLBACK: duas identidades, perfil de outro jogador, busca literal e sem distinção de caixa, paginação, validação de ícone e impedimento de alteração de XP pelo cliente.
- `tests/static_validation.py`: manifesto, todos os recursos, imagens, robots e entradas de compilação.
- Migração `geobr_profile_directory` aplicada no projeto atual. Permissões das novas funções revisadas; leitura da busca usa `security invoker`, atualização cosmética opera somente sobre `auth.uid()` e não recebe XP nem saldo.
- Conta HTTP temporária desconectada e removida. Conferência final: permanecem 1 perfil real, 107 Tokens e 625 XP pessoal; nenhum pedido fictício permanece.

Os testes completos das 64 quests foram executados na Beta 1.7. O algoritmo dessas quests não foi alterado nesta versão; não foram repetidos aqui.

## Limites práticos

Não há Geometry Dash em execução nem aparelho/emulador GDPS neste ambiente. A build foi compilada, e os fluxos da API foram testados; **a interface e os toques ainda precisam de validação no jogo**, seguindo `BETA_TESTING.md`. Compilar não garante ausência de crashes na combinação de mods do aparelho.

A busca lista perfis que já abriram o mod e sincronizaram com o Supabase GeoBR. Não varre todas as contas de `geobr.ps.fhgdps.com`, não comprova titularidade da conta GDPS e não mostra Tokens ou inventário de outras pessoas. Temas aparecem no Perfil GeoBR; o perfil nativo próprio mantém a faixa de nome temática.

Eventos de gameplay continuam informados pelo cliente; não foi adicionado antitrapaça por replay. A sessão anônima GeoBR não tem recuperação automática após apagar os dados do mod.
