# Supabase — GeoBR Beta 1.8

Projeto: `nolfgjsnnleqpmzvmebo`. As migrações abaixo **já foram aplicadas**; não execute novamente os `CREATE` no Dashboard:

- `20260908111953_geobr_quest_sequence_tier_shop.sql`
- `20260908114442_geobr_v17_interface_contract.sql`
- `20260909000709_geobr_profile_directory.sql`

Elas preservam Main Levels, notícias, comunidade, carteiras, XP e prêmios existentes. A pasta de migrações guarda o histórico, inclusive instalações antigas feitas manualmente; não é um script de reset portátil.

## Perfis da Beta 1.8

`geobr_profile_sync(p_name,p_icon,p_color1,p_color2)` sincroniza apenas o nome e o avatar da sessão atual, retornando o estado atualizado. Cores são inteiros RGB; XP, tema comprado e títulos continuam protegidos.

`geobr_players(p_query,p_offset)` permite buscar nomes, sem diferenciar maiúsculas de minúsculas, e retorna três perfis por página e o total. A busca trata `%` e `_` literalmente. Retorna apenas dados públicos de apresentação. A pessoa não precisa enviar mensagem no chat para aparecer: basta abrir esta versão do mod e a sincronização terminar.

O GDPS usado pelo projeto é `https://geobr.ps.fhgdps.com/`. A busca usa os perfis cadastrados pelo mod no Supabase. Ela não verifica a titularidade de contas FHGDPS e não consulta a lista de usuários do site. Nomes iguais não são mesclados; cada perfil usa a sua identidade GeoBR.

## Contrato do GeoPass

- `geobr_state_v2()` retorna temporada, XP do passe, Tokens, prêmios, **uma** quest, catálogo da loja, perfil e revisão crescente.
- `geobr_activity(p_event)` recebe tipo, porcentagem, dificuldade, modo, origem, formato, level, UUID da tentativa, UUID do evento e UUID da quest atribuída. Calcula o progresso e concede XP/Tokens uma vez. O Main Level é conferido no catálogo do servidor.
- `geobr_reward_v2(p_reward_id)` resgata prêmio liberado e retorna o novo estado. Mantém os recibos de entrega nativa.
- `geobr_buy_item(p_item_id,p_request_id,p_expected_price,p_expected_run,p_expected_tier)` verifica preço, saldo, quest/etapa e identificação da compra. Débito e benefício são atômicos. Repetir o mesmo pedido não cobra novamente.
- `geobr_equip_item(p_item_id)` exige propriedade do cosmético. `geobr_profile_name(p_name)` atualiza o nome exibido da identidade atual.
- `geobr_profiles` permite ler os cosméticos e XP público; não permite escrita direta. `geobr_inventory` só é visível ao proprietário.
- `geobr_bulletins` é uma view `security_invoker`, unindo notícias publicadas e avisos ainda válidos.

O catálogo ADM agora usa `geobr_quest_steps` e `geobr_shop_items`. Preços e quantidades nunca vêm de uma escrita direta do cliente à carteira. Implementações privilegiadas ficam no schema privado, com `search_path` fixo, verificação de `auth.uid()` e permissões explícitas.

Os dados privados de execução guardam a definição da quest, os eventos recebidos, levels/tentativas distintos, histórico de conclusão e pedidos de compra. O UUID da rodada evita que um evento atrasado avance outra quest. Bloqueios por usuário serializam alterações de XP e Tokens.

## Sessão e atualização global

Só a chave publicável acompanha o cliente. A senha ADM continua verificada pelo servidor e seu texto não está no binário ou nos fontes de produção. A sessão ADM tem vinte minutos e termina ao fechar o painel.

Menus consultam atualizações a cada vinte segundos; comunidade a cada dez. As alterações de catálogo aparecem sem nova versão do mod. Anúncios notificam nos menus enquanto válidos. Canais Realtime não são necessários para estas telas.

A autenticação anônima é intencional. Os avisos do advisor sobre essa modalidade são avaliados junto às regras por proprietário e à sessão ADM. Tabelas privadas têm RLS sem políticas públicas e acesso revogado; isso é intencional. Os grants de Collab permanecem revogados.

## Testes e limites

`tests/backend-v17.sql` cria duas identidades temporárias e reverte tudo ao terminar. Testa as 64 quests, repetição de eventos, retorno da sequência ao início, 46% exato, dois Autos distintos, metadados de Main Level, XP permanente, compras, skips, inventário, isolamento e cópia de temporadas futuras.

A sessão de teste anterior também verificou resgates de prêmios repetidos, sem duplicação. Os testes antigos de quests diárias em `supabase_geopass.sql` são históricos e não devem ser usados como contrato 1.7.

Os fatos de gameplay são informados pelo cliente. Esta versão não oferece validação antitrapaça de replay no servidor nem vínculo verificado com a conta GD. O servidor valida quantidades, preços, ownership, deduplicação e transações, não a legitimidade física de uma tentativa.
