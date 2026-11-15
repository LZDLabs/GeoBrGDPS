// GeoBR JSON endpoints used by the mod: account info, recovery codes, owner
// claim, linking the GDPS account to the GeoBR (Supabase) profile, Demon List.
import { type Ctx, attempt, audit, can, failures, grantsOf, jsonAccount } from "../lib/core.ts";
import { clean, gjp2Of } from "../lib/gd.ts";
import { hashSecret, validPassword } from "./accounts.ts";

const CODE_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

function newRecoveryCode(): string {
  const bytes = crypto.getRandomValues(new Uint8Array(16));
  const chars = Array.from(bytes, (b) => CODE_ALPHABET[b % CODE_ALPHABET.length]).join("");
  return chars.match(/.{4}/g)!.join("-");
}
const normalizeCode = (code: unknown) => String(code ?? "").toUpperCase().replace(/[^A-Z0-9]/g, "");

export async function me(ctx: Ctx) {
  const account = await jsonAccount(ctx);
  if (!account) return { ok: false, error: "Conta ou senha incorreta." };
  const g = await grantsOf(ctx, account.account_id);
  const [row] = await ctx.sql`select recovery_hash is not null as has_recovery, supabase_user is not null as linked, lb_banned from geobr_gdps.accounts where account_id = ${account.account_id}`;
  const [owner] = await ctx.sql`select exists(select 1 from geobr_gdps.account_roles ar join geobr_gdps.roles r using (role_id) where r.is_owner) as taken`;
  return {
    ok: true,
    account: { id: account.account_id, username: account.username, linked: row.linked, has_recovery: row.has_recovery, lb_banned: row.lb_banned },
    roles: g.roles,
    permissions: [...g.permissions].sort(),
    owner: g.owner,
    owner_available: !owner.taken,
  };
}

export async function recoveryNew(ctx: Ctx) {
  const account = await jsonAccount(ctx);
  if (!account) return { ok: false, error: "Conta ou senha incorreta." };
  const code = newRecoveryCode();
  const [row] = await ctx.sql`select extensions.crypt(${normalizeCode(code)}, extensions.gen_salt('bf', 8)) as h`;
  await ctx.sql`update geobr_gdps.accounts set recovery_hash = ${row.h}, recovery_created_at = now() where account_id = ${account.account_id}`;
  return { ok: true, code };
}

export async function recoveryReset(ctx: Ctx) {
  const username = String(ctx.json.userName ?? "").trim();
  const password = String(ctx.json.password ?? "");
  const code = normalizeCode(ctx.json.code);
  const subject = username.toLowerCase();
  if (!/^[a-z0-9]{3,20}$/.test(subject) || code.length !== 16) return { ok: false, error: "Dados invalidos." };
  if (await failures(ctx, "recovery", subject, 60) >= 5) return { ok: false, error: "Muitas tentativas. Tente de novo em 1 hora." };
  const bad = validPassword(password);
  if (bad) return { ok: false, error: "A senha precisa ter de 6 a 20 caracteres, sem espacos." };
  const [row] = await ctx.sql`
    select account_id::int, recovery_hash is not null and extensions.crypt(${code}, recovery_hash) = recovery_hash as ok
    from geobr_gdps.accounts where lower(username) = ${subject}`;
  if (!row?.ok) { await attempt(ctx, "recovery", subject, false); return { ok: false, error: "Usuario ou codigo incorreto." }; }
  const hash = await hashSecret(ctx, await gjp2Of(password));
  await ctx.sql`update geobr_gdps.accounts set gjp2_hash = ${hash}, recovery_hash = null, recovery_created_at = null where account_id = ${row.account_id}`;
  await attempt(ctx, "recovery", subject, true);
  await audit(ctx, row.account_id, "account.recover", "account", row.account_id);
  return { ok: true };
}

/** Password change for the logged-in account (GD has no in-game screen for it). */
export async function passwordChange(ctx: Ctx) {
  const account = await jsonAccount(ctx);
  if (!account) return { ok: false, error: "Senha atual incorreta. Entre de novo na conta." };
  const password = String(ctx.json.password ?? "");
  if (validPassword(password)) return { ok: false, error: "A senha precisa ter de 6 a 20 caracteres, sem espacos." };
  const gjp2 = await gjp2Of(password);
  const hash = await hashSecret(ctx, gjp2);
  await ctx.sql`update geobr_gdps.accounts set gjp2_hash = ${hash} where account_id = ${account.account_id}`;
  await audit(ctx, account.account_id, "account.password", "account", account.account_id);
  return { ok: true, gjp2 };
}

export async function ownerClaim(ctx: Ctx) {
  const account = await jsonAccount(ctx);
  if (!account) return { ok: false, error: "Conta ou senha incorreta." };
  const subject = String(account.account_id);
  if (await failures(ctx, "owner", subject, 60) >= 5) return { ok: false, error: "Muitas tentativas. Tente de novo em 1 hora." };
  const [taken] = await ctx.sql`select 1 from geobr_gdps.account_roles ar join geobr_gdps.roles r using (role_id) where r.is_owner limit 1`;
  if (taken) return { ok: false, error: "O servidor ja tem um Owner." };
  const [check] = await ctx.sql`
    select coalesce((select extensions.crypt(${String(ctx.json.adminPassword ?? "")}, password_hash) = password_hash from geobr_private.admin_secret limit 1), false) as ok`;
  if (!check.ok) { await attempt(ctx, "owner", subject, false); return { ok: false, error: "Senha ADM incorreta." }; }
  await attempt(ctx, "owner", subject, true);
  await ctx.sql`
    insert into geobr_gdps.account_roles (account_id, role_id, granted_by)
    select ${account.account_id}, role_id, ${account.account_id} from geobr_gdps.roles where is_owner order by role_id limit 1
    on conflict do nothing`;
  await audit(ctx, account.account_id, "owner.claim", "account", account.account_id);
  return { ok: true };
}

// ------------------------------------------------ GeoBR profile <-> GDPS
async function gotrue(ctx: Ctx, method: string, path: string, body?: unknown, bearer?: string) {
  const res = await fetch(`${ctx.env.supabaseUrl}/auth/v1${path}`, {
    method,
    headers: { apikey: ctx.env.serviceKey, Authorization: `Bearer ${bearer ?? ctx.env.serviceKey}`, "Content-Type": "application/json" },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const text = await res.text();
  let json: Record<string, unknown> = {};
  try { json = text ? JSON.parse(text) : {}; } catch { /* not json */ }
  return { ok: res.ok, status: res.status, json };
}

/** A fresh session for an existing Supabase user (admin magic link, never e-mailed). */
async function sessionFor(ctx: Ctx, userId: string, accountId: number) {
  const user = await gotrue(ctx, "GET", `/admin/users/${userId}`);
  if (!user.ok) return null;
  let email = String(user.json.email ?? "");
  if (!email) {
    email = `gdps-${accountId}@geobr-gdps.invalid`; // reserved TLD: nothing can ever be delivered
    const upd = await gotrue(ctx, "PUT", `/admin/users/${userId}`, { email, email_confirm: true });
    if (!upd.ok) { console.error("gotrue update", upd.status, JSON.stringify(upd.json).slice(0, 200)); return null; }
  }
  const link = await gotrue(ctx, "POST", "/admin/generate_link", { type: "magiclink", email });
  const props = (link.json.properties ?? {}) as Record<string, unknown>;
  const tokenHash = String(link.json.hashed_token ?? props.hashed_token ?? "");
  if (!link.ok || !tokenHash) { console.error("gotrue link", link.status); return null; }
  const verify = await gotrue(ctx, "POST", "/verify", { type: "magiclink", token_hash: tokenHash });
  if (!verify.ok || !verify.json.refresh_token) { console.error("gotrue verify", verify.status); return null; }
  return { access_token: String(verify.json.access_token), refresh_token: String(verify.json.refresh_token), user_id: userId };
}

export async function session(ctx: Ctx) {
  const account = await jsonAccount(ctx);
  if (!account) return { ok: false, error: "Conta ou senha incorreta." };
  const token = String(ctx.json.accessToken ?? "");
  if (!ctx.env.serviceKey) return { ok: false, error: "Servidor sem chave de servico." };
  const me = token ? await gotrue(ctx, "GET", "/user", undefined, token) : { ok: false, json: {} as Record<string, unknown> };
  const deviceUser = me.ok ? String(me.json.id ?? "") : "";
  if (account.supabase_user) {
    if (account.supabase_user === deviceUser) return { ok: true, same: true };
    const s = await sessionFor(ctx, account.supabase_user, account.account_id);
    if (!s) return { ok: false, error: "Nao foi possivel abrir o perfil GeoBR desta conta." };
    return { ok: true, same: false, ...s };
  }
  if (!deviceUser) return { ok: false, error: "Sessao GeoBR invalida neste aparelho." };
  const [other] = await ctx.sql`select account_id::int, username from geobr_gdps.accounts where supabase_user = ${deviceUser}::uuid`;
  if (other) return { ok: true, same: false, needs_new_profile: true, linked_to: other.username };
  await ctx.sql`update geobr_gdps.accounts set supabase_user = ${deviceUser}::uuid where account_id = ${account.account_id} and supabase_user is null`;
  return { ok: true, same: true, linked: true };
}

export async function demonList(ctx: Ctx) {
  const rows = await ctx.sql`
    select d.position, d.level_id::int as level_id, l.name, a.username as creator, d.verifier, l.demon_diff
    from geobr_gdps.demon_list d join geobr_gdps.levels l on l.level_id = d.level_id
    join geobr_gdps.accounts a on a.account_id = l.account_id
    where not l.deleted and not l.blocked order by d.position, d.level_id limit 200`;
  return { ok: true, levels: rows.map((r) => ({ ...r, name: clean(r.name, 20), verifier: clean(r.verifier, 30) })) };
}

export const _test = { newRecoveryCode, normalizeCode, can };
