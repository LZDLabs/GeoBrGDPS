# Validação RC2

SHA-256: `7eec03fb6da12df967f7501838b411944ec789dc4d6c75562f6af86af983d1d7`

- Compilação Android64 Release: PASS.
- Integridade ZIP/ELF AArch64/manifesto/recursos: PASS.
- Limites de layout: PASS.
- Testes de servidor original (live_v1_regression.sql): PASS, ROLLBACK.
- Temporada ativa: 30 etapas, 12 skins, 24 quests.
- Sem usuários de teste remanescentes.
- Avatar/custom icon e layout: precisam de confirmação no aparelho.
- Crash: fluxo de lançamento corrigido; sem reprodução do crash no GDPS neste ambiente.

Revisão Supabase: funções privilegiadas movidas para schema privado, wrappers públicos invoker. Permanecem avisos esperados sobre autenticação anônima e tabelas internas sem policies de acesso cliente; a autenticação anônima da Alpha foi preservada.
[Políticas de usuários anônimos](https://supabase.com/docs/guides/database/database-advisors?queryGroups=lint&lint=0012_auth_allow_anonymous_sign_ins)
[Tabelas internas com RLS](https://supabase.com/docs/guides/database/database-linter?lint=0008_rls_enabled_no_policy)
