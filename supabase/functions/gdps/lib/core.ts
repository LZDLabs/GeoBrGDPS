// Request context, account authentication (accountID + gjp2 against a bcrypt
// hash), role permissions and the audit log. Every permission check happens
// here on the server; the mod's UI only mirrors what the server allows.
import type postgres from "postgres";
import type { Store } from "./storage.ts";
import { sha256 } from "./gd.ts";

export type Sql = postgres.Sql<Record<string, never>>;

export interface Env {
  supabaseUrl: string;
  serviceKey: string;
  functionUrl: string;      // public base URL of this function (GD's account URL)
  songsBucket: string;
  savesBucket: string;
}

export interface Ctx {
  sql: Sql;
  store: Store;
  env: Env;
  params: URLSearchParams;  // GD form body (also query string)
  json: Record<string, unknown>;
  ip: string;
  now: Date;
}

export interface Account {
  account_id: number;
  username: string;
  is_banned: boolean;
  lb_banned: boolean;
  supabase_user: string | null;
}

export const ALL_PERMISSIONS = [
  "admin.panel", "accounts.view", "accounts.ban", "accounts.manage", "roles.manage",
  "levels.rate", "levels.feature", "levels.suggest", "levels.delete", "levels.block",
  "comments.moderate", "events.manage", "leaderboard.moderate", "demonlist.manage",
  "songs.moderate", "stats.view", "audit.view", "mod.badge", "mod.elder", "packs.manage",
] as const;
export type Permission = typeof ALL_PERMISSIONS[number];

export class HttpError extends Error {
  constructor(public status: number, message: string) { super(message); }
}

const verified = new Map<string, number>();
const VERIFY_TTL_MS = 120_000;

/** Checks accountID + gjp2. Banned accounts are refused unless `allowBanned`. */
export async function authenticate(ctx: Ctx, accountID: unknown, gjp2: unknown, allowBanned = false): Promise<Account | null> {
  const id = Number.parseInt(String(accountID ?? ""), 10);
  const secret = String(gjp2 ?? "");
  if (!Number.isFinite(id) || id <= 0 || !/^[0-9a-f]{40}$/.test(secret)) return null;
  const [row] = await ctx.sql`
    select account_id::int, username, gjp2_hash, is_banned, banned_until, lb_banned, supabase_user::text
    from geobr_gdps.accounts where account_id = ${id}`;
  if (!row) return null;
  const key = `${row.account_id}|${row.gjp2_hash}|${await sha256(secret)}`;
  const until = verified.get(key);
  if (!until || until < Date.now()) {
    const [check] = await ctx.sql`select extensions.crypt(${secret}, ${row.gjp2_hash}) = ${row.gjp2_hash} as ok`;
    if (!check?.ok) return null;
    if (verified.size > 5000) verified.clear();
    verified.set(key, Date.now() + VERIFY_TTL_MS);
  }
  let banned = row.is_banned as boolean;
  if (banned && row.banned_until && new Date(row.banned_until) <= ctx.now) {
    await ctx.sql`update geobr_gdps.accounts set is_banned = false, ban_reason = null, banned_until = null where account_id = ${id}`;
    banned = false;
  }
  if (banned && !allowBanned) return null;
  return { account_id: row.account_id, username: row.username, is_banned: banned, lb_banned: row.lb_banned, supabase_user: row.supabase_user };
}

/** GD requests carry accountID + gjp2 as form fields. */
export const gdAccount = (ctx: Ctx) => authenticate(ctx, ctx.params.get("accountID"), ctx.params.get("gjp2"));

/** GeoBR JSON requests carry them in the body. */
export const jsonAccount = (ctx: Ctx) => authenticate(ctx, ctx.json.accountID, ctx.json.gjp2);

export interface Grants {
  permissions: Set<string>;
  owner: boolean;
  topPosition: number;       // lowest position number held (0 = Owner)
  roles: Array<{ role_id: number; name: string; color: number; position: number }>;
}

export async function grantsOf(ctx: Ctx, accountId: number): Promise<Grants> {
  const rows = await ctx.sql`
    select r.role_id::int, r.name, r.color, r.position, r.is_owner, r.permissions
    from geobr_gdps.account_roles ar join geobr_gdps.roles r using (role_id)
    where ar.account_id = ${accountId} order by r.position, r.role_id`;
  const permissions = new Set<string>();
  let owner = false;
  let topPosition = 1_000_000;
  for (const r of rows) {
    owner ||= r.is_owner;
    topPosition = Math.min(topPosition, r.position);
    for (const p of r.permissions as string[]) {
      if (p === "*") ALL_PERMISSIONS.forEach((x) => permissions.add(x));
      else permissions.add(p);
    }
  }
  if (owner) ALL_PERMISSIONS.forEach((x) => permissions.add(x));
  return { permissions, owner, topPosition, roles: rows.map((r) => ({ role_id: r.role_id, name: r.name, color: r.color, position: r.position })) };
}

export const can = (g: Grants, p: Permission) => g.owner || g.permissions.has(p);

export async function audit(ctx: Ctx, actor: number | null, action: string, targetType: string, targetId: unknown, details: Record<string, unknown> = {}) {
  await ctx.sql`
    insert into geobr_gdps.audit_log (actor, action, target_type, target_id, details)
    values (${actor}, ${action}, ${targetType}, ${String(targetId ?? "")}, ${ctx.sql.json(details as postgres.JSONValue)})`;
}

/** Failed attempts of `kind` for `subject` inside the window. */
export async function failures(ctx: Ctx, kind: string, subject: string, minutes: number): Promise<number> {
  const [row] = await ctx.sql`
    select count(*)::int as n from geobr_gdps.auth_attempts
    where kind = ${kind} and subject = ${subject} and not ok and at > now() - make_interval(mins => ${minutes})`;
  return row?.n ?? 0;
}

export async function attempt(ctx: Ctx, kind: string, subject: string, ok: boolean) {
  await ctx.sql`insert into geobr_gdps.auth_attempts (kind, subject, ok) values (${kind}, ${subject}, ${ok})`;
}

export function param(ctx: Ctx, name: string, fallback = ""): string {
  return ctx.params.get(name) ?? fallback;
}

export function intParam(ctx: Ctx, name: string, fallback = 0): number {
  const n = Number.parseInt(ctx.params.get(name) ?? "", 10);
  return Number.isFinite(n) ? n : fallback;
}

/** Mod badge for key 49 / comments: 2 elder, 1 moderator, 3 leaderboard. */
export function badgeOf(g: Grants): number {
  if (can(g, "mod.elder")) return 2;
  if (can(g, "mod.badge")) return 1;
  if (can(g, "leaderboard.moderate")) return 3;
  return 0;
}

/** Badge and role colour for many accounts at once (comments, profiles). */
export async function badges(ctx: Ctx, ids: number[]): Promise<Map<number, { badge: number; color: number | null }>> {
  const out = new Map<number, { badge: number; color: number | null }>();
  if (!ids.length) return out;
  const rows = await ctx.sql`
    select ar.account_id::int, r.permissions, r.is_owner, r.color, r.position
    from geobr_gdps.account_roles ar join geobr_gdps.roles r using (role_id)
    where ar.account_id = any(string_to_array(${ids.join(",")}, ',')::bigint[])
    order by r.position`;
  const grouped = new Map<number, Grants>();
  const colour = new Map<number, number>();
  for (const r of rows) {
    const g = grouped.get(r.account_id) ?? { permissions: new Set<string>(), owner: false, topPosition: 1e6, roles: [] };
    g.owner ||= r.is_owner;
    for (const p of r.permissions as string[]) g.permissions.add(p === "*" ? "mod.elder" : p);
    if (r.is_owner) g.permissions.add("mod.elder");
    grouped.set(r.account_id, g);
    if (!colour.has(r.account_id)) colour.set(r.account_id, r.color);
  }
  for (const [id, g] of grouped) out.set(id, { badge: badgeOf(g), color: colour.get(id) ?? null });
  return out;
}

export function rgb(color: number | null | undefined): string {
  if (color == null) return "";
  return `${(color >> 16) & 255},${(color >> 8) & 255},${color & 255}`;
}
