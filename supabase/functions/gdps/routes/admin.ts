// GDPS admin panel. Each action names the permission it needs; the check is
// done here, per request, from the roles stored in the database. Every change
// is written to geobr_gdps.audit_log.
import { ALL_PERMISSIONS, type Ctx, type Grants, type Permission, audit, can, grantsOf, jsonAccount } from "../lib/core.ts";
import { clean, idList, parseRewards, safeInt } from "../lib/gd.ts";
import { recomputeCreatorPoints } from "./accounts.ts";
import { applyRating } from "./levels.ts";

// deno-lint-ignore no-explicit-any
type Json = Record<string, any>;
type Actor = { id: number; g: Grants };
type Action = { perm: Permission | Permission[]; run: (ctx: Ctx, actor: Actor, body: Json) => Promise<Json> };

const ok = (extra: Json = {}) => ({ ok: true, ...extra });
const fail = (error: string) => ({ ok: false, error });

/** "1, 2,3" -> the IDs (in order) of levels that exist and are not deleted. */
async function existingLevels(ctx: Ctx, value: unknown, cap: number): Promise<number[]> {
  const ids = idList(Array.isArray(value) ? value.join(",") : value, cap);
  if (!ids.length) return [];
  const rows = await ctx.sql`select level_id::int as id from geobr_gdps.levels where level_id = any(string_to_array(${ids.join(",")}, ',')::bigint[]) and not deleted`;
  const found = new Set(rows.map((r) => r.id as number));
  return ids.filter((id) => found.has(id));
}

/** "#ff8800", "255,136,0" or a number -> "r,g,b". */
function rgbText(value: unknown): string {
  const s = String(value ?? "").trim();
  const parts = s.split(",").map((x) => Number.parseInt(x, 10));
  if (parts.length === 3 && parts.every((x) => Number.isFinite(x))) return parts.map((x) => Math.min(255, Math.max(0, x))).join(",");
  const hex = Number.parseInt(s.replace("#", ""), 16);
  if (Number.isFinite(hex)) return [(hex >> 16) & 255, (hex >> 8) & 255, hex & 255].join(",");
  return "255,255,255";
}

async function targetPosition(ctx: Ctx, accountId: number): Promise<{ position: number; owner: boolean } | null> {
  const [row] = await ctx.sql`
    select a.account_id, coalesce(min(r.position), 1000000)::int as position, coalesce(bool_or(r.is_owner), false) as owner
    from geobr_gdps.accounts a left join geobr_gdps.account_roles ar on ar.account_id = a.account_id
    left join geobr_gdps.roles r on r.role_id = ar.role_id where a.account_id = ${accountId} group by a.account_id`;
  return row ? { position: row.position, owner: row.owner } : null;
}

/** Staff can only act on accounts strictly below them; the Owner on anyone but other owners. */
async function outranks(ctx: Ctx, actor: Actor, target: number): Promise<string | null> {
  if (target === actor.id) return "Voce nao pode fazer isso com a sua propria conta.";
  const t = await targetPosition(ctx, target);
  if (!t) return "Conta nao encontrada.";
  if (t.owner) return "Nao e possivel moderar um Owner.";
  if (!actor.g.owner && t.position <= actor.g.topPosition) return "Esse jogador tem um cargo igual ou acima do seu.";
  return null;
}

function accountJson(a: Json) {
  return {
    id: Number(a.account_id), username: a.username, created_at: a.created_at, last_login: a.last_login,
    is_banned: a.is_banned, ban_reason: a.ban_reason, banned_until: a.banned_until,
    lb_banned: a.lb_banned, lb_ban_reason: a.lb_ban_reason,
    stars: a.stars, moons: a.moons, demons: a.demons, diamonds: a.diamonds, coins: a.coins, user_coins: a.user_coins,
    creator_points: a.creator_points,
  };
}

const actions: Record<string, Action> = {
  // ------------------------------------------------------------- overview
  stats: { perm: "stats.view", run: async (ctx) => {
    const [s] = await ctx.sql`select
      (select count(*)::int from geobr_gdps.accounts) as accounts,
      (select count(*)::int from geobr_gdps.accounts where last_login > now() - interval '1 day') as active_day,
      (select count(*)::int from geobr_gdps.accounts where last_login > now() - interval '7 days') as active_week,
      (select count(*)::int from geobr_gdps.accounts where is_banned) as banned,
      (select count(*)::int from geobr_gdps.accounts where lb_banned) as lb_banned,
      (select count(*)::int from geobr_gdps.levels where not deleted) as levels,
      (select count(*)::int from geobr_gdps.levels where not deleted and stars > 0) as rated,
      (select count(*)::int from geobr_gdps.levels where not deleted and feature_score > 0) as featured,
      (select count(*)::int from geobr_gdps.levels where not deleted and epic > 0) as epic,
      (select count(*)::int from geobr_gdps.levels where blocked) as blocked,
      (select count(*)::int from geobr_gdps.levels where created_at > now() - interval '1 day') as levels_day,
      (select count(*)::int from geobr_gdps.comments where not deleted) as comments,
      (select count(*)::int from geobr_gdps.songs where status = 'approved') as songs,
      (select count(*)::int from geobr_gdps.songs where status = 'pending') as songs_pending,
      (select count(*)::int from geobr_gdps.level_reports where at > now() - interval '7 days') as reports_week,
      (select count(*)::int from geobr_gdps.stat_history where flagged and at > now() - interval '7 days') as flagged_week,
      (select coalesce(sum(downloads), 0)::bigint from geobr_gdps.levels) as downloads`;
    return ok({ stats: { ...s, downloads: Number(s.downloads) } });
  } },

  // ------------------------------------------------------------- accounts
  "accounts.search": { perm: "accounts.view", run: async (ctx, _a, b) => {
    const q = String(b.q ?? "").trim().slice(0, 20);
    const asId = /^\d+$/.test(q) ? Number(q) : -1;
    const rows = await ctx.sql`
      select * from geobr_gdps.accounts
      where ${q === ""} or account_id = ${asId} or lower(username) like ${"%" + q.toLowerCase().replace(/[%_\\]/g, "") + "%"}
      order by (lower(username) = ${q.toLowerCase()}) desc, account_id desc limit 30`;
    return ok({ accounts: rows.map(accountJson) });
  } },
  "accounts.get": { perm: "accounts.view", run: async (ctx, _a, b) => {
    const id = safeInt(b.target, 0);
    const [a] = await ctx.sql`select * from geobr_gdps.accounts where account_id = ${id}`;
    if (!a) return fail("Conta nao encontrada.");
    const g = await grantsOf(ctx, id);
    const [counts] = await ctx.sql`select
      (select count(*)::int from geobr_gdps.levels where account_id = ${id} and not deleted) as levels,
      (select count(*)::int from geobr_gdps.comments where account_id = ${id} and not deleted) as comments`;
    return ok({ account: { ...accountJson(a), roles: g.roles, ...counts } });
  } },
  "accounts.ban": { perm: "accounts.ban", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0);
    const why = await outranks(ctx, actor, id);
    if (why) return fail(why);
    const reason = clean(b.reason, 120);
    const days = safeInt(b.days, 0, 0, 3650);
    await ctx.sql`update geobr_gdps.accounts set is_banned = true, ban_reason = ${reason},
      banned_until = case when ${days} > 0 then now() + make_interval(days => ${days}) else null end where account_id = ${id}`;
    await audit(ctx, actor.id, "account.ban", "account", id, { reason, days });
    return ok();
  } },
  "accounts.unban": { perm: "accounts.ban", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0);
    const res = await ctx.sql`update geobr_gdps.accounts set is_banned = false, ban_reason = null, banned_until = null where account_id = ${id}`;
    if (!res.count) return fail("Conta nao encontrada.");
    await audit(ctx, actor.id, "account.unban", "account", id);
    return ok();
  } },
  "accounts.rename": { perm: "accounts.manage", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0);
    const name = String(b.username ?? "").trim();
    if (!/^[A-Za-z0-9]{3,20}$/.test(name)) return fail("Nome invalido (3 a 20 letras ou numeros).");
    const why = await outranks(ctx, actor, id);
    if (why) return fail(why);
    const [taken] = await ctx.sql`select 1 from geobr_gdps.accounts where lower(username) = lower(${name}) and account_id <> ${id}`;
    if (taken) return fail("Esse nome ja esta em uso.");
    const [before] = await ctx.sql`select username from geobr_gdps.accounts where account_id = ${id}`;
    await ctx.sql`update geobr_gdps.accounts set username = ${name} where account_id = ${id}`;
    await audit(ctx, actor.id, "account.rename", "account", id, { from: before?.username, to: name });
    return ok();
  } },

  // ---------------------------------------------------------- leaderboard
  "lb.ban": { perm: "leaderboard.moderate", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0);
    const why = await outranks(ctx, actor, id);
    if (why) return fail(why);
    const reason = clean(b.reason, 120);
    await ctx.sql`update geobr_gdps.accounts set lb_banned = true, lb_ban_reason = ${reason} where account_id = ${id}`;
    await audit(ctx, actor.id, "leaderboard.ban", "account", id, { reason });
    return ok();
  } },
  "lb.unban": { perm: "leaderboard.moderate", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0);
    await ctx.sql`update geobr_gdps.accounts set lb_banned = false, lb_ban_reason = null where account_id = ${id}`;
    await audit(ctx, actor.id, "leaderboard.unban", "account", id);
    return ok();
  } },
  "lb.history": { perm: "leaderboard.moderate", run: async (ctx, _a, b) => {
    const rows = await ctx.sql`select stars, moons, demons, diamonds, coins, user_coins, flagged, at from geobr_gdps.stat_history
      where account_id = ${safeInt(b.target, 0)} order by at desc limit 50`;
    return ok({ history: rows });
  } },
  "lb.flagged": { perm: "leaderboard.moderate", run: async (ctx) => {
    const rows = await ctx.sql`
      select h.account_id::int as id, a.username, h.stars, h.demons, h.moons, h.user_coins, h.at, a.lb_banned
      from geobr_gdps.stat_history h join geobr_gdps.accounts a using (account_id)
      where h.flagged order by h.at desc limit 50`;
    return ok({ flagged: rows });
  } },
  "lb.reset": { perm: "leaderboard.moderate", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0);
    const why = await outranks(ctx, actor, id);
    if (why) return fail(why);
    const [before] = await ctx.sql`select stars, moons, demons, diamonds, coins, user_coins from geobr_gdps.accounts where account_id = ${id}`;
    await ctx.sql`update geobr_gdps.accounts set stars = 0, moons = 0, demons = 0, diamonds = 0, coins = 0, user_coins = 0 where account_id = ${id}`;
    await audit(ctx, actor.id, "leaderboard.reset", "account", id, { before });
    return ok();
  } },
  "scores.list": { perm: "leaderboard.moderate", run: async (ctx, _a, b) => {
    const rows = await ctx.sql`
      select s.account_id::int as id, a.username, s.percent, s.attempts, s.clicks, s.seconds, s.coins, s.plat_time, s.plat_points, s.updated_at
      from geobr_gdps.level_scores s join geobr_gdps.accounts a using (account_id)
      where s.level_id = ${safeInt(b.levelID, 0)} order by s.percent desc, s.plat_points desc limit 100`;
    return ok({ scores: rows });
  } },
  "scores.remove": { perm: "leaderboard.moderate", run: async (ctx, actor, b) => {
    const levelId = safeInt(b.levelID, 0), id = safeInt(b.target, 0);
    const [row] = await ctx.sql`delete from geobr_gdps.level_scores where level_id = ${levelId} and account_id = ${id} returning percent, plat_time, plat_points`;
    if (!row) return fail("Pontuacao nao encontrada.");
    await audit(ctx, actor.id, "score.remove", "level", levelId, { account: id, removed: row });
    return ok();
  } },

  // ---------------------------------------------------------------- roles
  "roles.list": { perm: "admin.panel", run: async (ctx) => {
    const rows = await ctx.sql`
      select r.role_id::int, r.name, r.color, r.permissions, r.position, r.is_owner,
        (select count(*)::int from geobr_gdps.account_roles ar where ar.role_id = r.role_id) as members
      from geobr_gdps.roles r order by r.position, r.role_id`;
    return ok({ roles: rows, catalog: ALL_PERMISSIONS });
  } },
  "roles.members": { perm: "admin.panel", run: async (ctx, _a, b) => {
    const rows = await ctx.sql`select a.account_id::int as id, a.username from geobr_gdps.account_roles ar join geobr_gdps.accounts a using (account_id)
      where ar.role_id = ${safeInt(b.role_id, 0)} order by a.username limit 200`;
    return ok({ members: rows });
  } },
  "roles.save": { perm: "roles.manage", run: async (ctx, actor, b) => {
    const name = clean(b.name, 32).trim();
    if (name.length < 2) return fail("Nome do cargo: 2 a 32 caracteres.");
    const color = safeInt(b.color, 16777215, 0, 16777215);
    const position = safeInt(b.position, 100, 1, 1000);
    const perms = [...new Set((Array.isArray(b.permissions) ? b.permissions : []).map(String))].filter((p) => (ALL_PERMISSIONS as readonly string[]).includes(p));
    if (!actor.g.owner) {
      if (position <= actor.g.topPosition) return fail("O cargo precisa ficar abaixo do seu.");
      const extra = perms.filter((p) => !actor.g.permissions.has(p));
      if (extra.length) return fail(`Voce nao pode dar permissoes que nao tem: ${extra.join(", ")}`);
    }
    const roleId = safeInt(b.role_id, 0);
    try {
      if (roleId > 0) {
        const [r] = await ctx.sql`select is_owner, position from geobr_gdps.roles where role_id = ${roleId}`;
        if (!r) return fail("Cargo nao encontrado.");
        if (r.is_owner) {
          if (!actor.g.owner) return fail("Somente o Owner edita o cargo Owner.");
          await ctx.sql`update geobr_gdps.roles set name = ${name}, color = ${color} where role_id = ${roleId}`;
        } else {
          if (!actor.g.owner && r.position <= actor.g.topPosition) return fail("Esse cargo esta acima do seu.");
          await ctx.sql`update geobr_gdps.roles set name = ${name}, color = ${color}, position = ${position}, permissions = ${perms} where role_id = ${roleId}`;
        }
        await audit(ctx, actor.id, "role.update", "role", roleId, { name, color, position, permissions: perms });
        return ok({ role_id: roleId });
      }
      const [row] = await ctx.sql`insert into geobr_gdps.roles (name, color, permissions, position) values (${name}, ${color}, ${perms}, ${position}) returning role_id::int`;
      await audit(ctx, actor.id, "role.create", "role", row.role_id, { name, color, position, permissions: perms });
      return ok({ role_id: row.role_id });
    } catch (e) {
      if ((e as { code?: string }).code === "23505") return fail("Ja existe um cargo com esse nome.");
      throw e;
    }
  } },
  "roles.delete": { perm: "roles.manage", run: async (ctx, actor, b) => {
    const roleId = safeInt(b.role_id, 0);
    const [r] = await ctx.sql`select name, is_owner, position from geobr_gdps.roles where role_id = ${roleId}`;
    if (!r) return fail("Cargo nao encontrado.");
    if (r.is_owner) return fail("O cargo Owner nao pode ser apagado.");
    if (!actor.g.owner && r.position <= actor.g.topPosition) return fail("Esse cargo esta acima do seu.");
    await ctx.sql`delete from geobr_gdps.roles where role_id = ${roleId}`;
    await audit(ctx, actor.id, "role.delete", "role", roleId, { name: r.name });
    return ok();
  } },
  "roles.grant": { perm: "roles.manage", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0), roleId = safeInt(b.role_id, 0);
    const [r] = await ctx.sql`select name, is_owner, position from geobr_gdps.roles where role_id = ${roleId}`;
    const [t] = await ctx.sql`select 1 from geobr_gdps.accounts where account_id = ${id}`;
    if (!r || !t) return fail("Cargo ou conta nao encontrados.");
    if (r.is_owner ? !actor.g.owner : (!actor.g.owner && r.position <= actor.g.topPosition)) return fail("Voce nao pode dar esse cargo.");
    await ctx.sql`insert into geobr_gdps.account_roles (account_id, role_id, granted_by) values (${id}, ${roleId}, ${actor.id}) on conflict do nothing`;
    await audit(ctx, actor.id, "role.grant", "account", id, { role: r.name });
    return ok();
  } },
  "roles.revoke": { perm: "roles.manage", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0), roleId = safeInt(b.role_id, 0);
    const [r] = await ctx.sql`select name, is_owner, position from geobr_gdps.roles where role_id = ${roleId}`;
    if (!r) return fail("Cargo nao encontrado.");
    if (r.is_owner) {
      if (!actor.g.owner) return fail("Somente o Owner mexe no cargo Owner.");
      const [n] = await ctx.sql`select count(*)::int as n from geobr_gdps.account_roles where role_id = ${roleId}`;
      if (n.n <= 1) return fail("O servidor precisa de pelo menos um Owner.");
    } else if (!actor.g.owner && r.position <= actor.g.topPosition) return fail("Voce nao pode tirar esse cargo.");
    const res = await ctx.sql`delete from geobr_gdps.account_roles where account_id = ${id} and role_id = ${roleId}`;
    if (!res.count) return fail("A conta nao tem esse cargo.");
    await audit(ctx, actor.id, "role.revoke", "account", id, { role: r.name });
    return ok();
  } },

  // --------------------------------------------------------------- levels
  "levels.search": { perm: "admin.panel", run: async (ctx, _a, b) => {
    const q = String(b.q ?? "").trim().slice(0, 20);
    const asId = /^\d+$/.test(q) ? Number(q) : -1;
    const rows = await ctx.sql`
      select l.level_id::int as id, l.name, a.username as creator, l.account_id::int as creator_id, l.stars, l.demon, l.demon_diff,
        l.feature_score, l.epic, l.coins, l.coins_verified, l.downloads, l.likes, l.deleted, l.blocked, l.block_reason, l.unlisted,
        l.requested_stars, (select count(*)::int from geobr_gdps.level_reports r where r.level_id = l.level_id) as reports
      from geobr_gdps.levels l join geobr_gdps.accounts a on a.account_id = l.account_id
      where ${q === ""} or l.level_id = ${asId} or lower(l.name) like ${"%" + q.toLowerCase().replace(/[%_\\]/g, "") + "%"} or lower(a.username) = ${q.toLowerCase()}
      order by (l.level_id = ${asId}) desc, l.level_id desc limit 30`;
    return ok({ levels: rows });
  } },
  "levels.rate": { perm: "levels.rate", run: async (ctx, actor, b) => {
    const done = await applyRating(ctx, actor.id, safeInt(b.levelID, 0), {
      stars: safeInt(b.stars, 0, 0, 10),
      demonDiff: b.demonDiff === undefined ? undefined : safeInt(b.demonDiff, 3, 1, 5),
      coins: b.coins === undefined ? undefined : !!b.coins,
    });
    return done ? ok() : fail("Level nao encontrado.");
  } },
  "levels.feature": { perm: "levels.feature", run: async (ctx, actor, b) => {
    const levelId = safeInt(b.levelID, 0);
    const [l] = await ctx.sql`select stars from geobr_gdps.levels where level_id = ${levelId} and not deleted`;
    if (!l) return fail("Level nao encontrado.");
    if (l.stars === 0) return fail("Avalie o level (estrelas) antes de dar Feature/Epic.");
    const done = await applyRating(ctx, actor.id, levelId, { stars: l.stars, feature: !!b.feature || safeInt(b.epic, 0) > 0, epic: safeInt(b.epic, 0, 0, 3) });
    return done ? ok() : fail("Level nao encontrado.");
  } },
  "levels.delete": { perm: "levels.delete", run: async (ctx, actor, b) => {
    const levelId = safeInt(b.levelID, 0);
    const [l] = await ctx.sql`update geobr_gdps.levels set deleted = true, updated_at = now() where level_id = ${levelId} returning account_id::int, name`;
    if (!l) return fail("Level nao encontrado.");
    await recomputeCreatorPoints(ctx, l.account_id);
    await audit(ctx, actor.id, "level.delete", "level", levelId, { name: l.name, reason: clean(b.reason, 120) });
    return ok();
  } },
  "levels.restore": { perm: "levels.delete", run: async (ctx, actor, b) => {
    const levelId = safeInt(b.levelID, 0);
    const [l] = await ctx.sql`update geobr_gdps.levels set deleted = false, updated_at = now() where level_id = ${levelId} returning account_id::int`;
    if (!l) return fail("Level nao encontrado.");
    await recomputeCreatorPoints(ctx, l.account_id);
    await audit(ctx, actor.id, "level.restore", "level", levelId);
    return ok();
  } },
  "levels.block": { perm: "levels.block", run: async (ctx, actor, b) => {
    const levelId = safeInt(b.levelID, 0);
    const reason = clean(b.reason, 120);
    const [l] = await ctx.sql`update geobr_gdps.levels set blocked = true, block_reason = ${reason}, updated_at = now() where level_id = ${levelId} returning account_id::int`;
    if (!l) return fail("Level nao encontrado.");
    await recomputeCreatorPoints(ctx, l.account_id);
    await audit(ctx, actor.id, "level.block", "level", levelId, { reason });
    return ok();
  } },
  "levels.unblock": { perm: "levels.block", run: async (ctx, actor, b) => {
    const levelId = safeInt(b.levelID, 0);
    const [l] = await ctx.sql`update geobr_gdps.levels set blocked = false, block_reason = null, updated_at = now() where level_id = ${levelId} returning account_id::int`;
    if (!l) return fail("Level nao encontrado.");
    await recomputeCreatorPoints(ctx, l.account_id);
    await audit(ctx, actor.id, "level.unblock", "level", levelId);
    return ok();
  } },
  "levels.reports": { perm: ["levels.block", "levels.delete"], run: async (ctx) => {
    const rows = await ctx.sql`
      select l.level_id::int as id, l.name, a.username as creator, count(*)::int as reports, max(r.at) as last
      from geobr_gdps.level_reports r join geobr_gdps.levels l using (level_id) join geobr_gdps.accounts a on a.account_id = l.account_id
      where not l.deleted and not l.blocked group by 1, 2, 3 order by reports desc, last desc limit 30`;
    return ok({ levels: rows });
  } },
  "suggestions.list": { perm: "levels.rate", run: async (ctx) => {
    const rows = await ctx.sql`
      select l.level_id::int as id, l.name, round(avg(s.stars))::int as stars, bool_or(s.feature) as feature, count(*)::int as votes,
        string_agg(distinct m.username, ', ') as by
      from geobr_gdps.suggestions s join geobr_gdps.levels l using (level_id) join geobr_gdps.accounts m on m.account_id = s.account_id
      where l.stars = 0 and not l.deleted group by 1, 2 order by max(s.id) desc limit 30`;
    return ok({ levels: rows });
  } },

  // ------------------------------------------------------------- comments
  "comments.recent": { perm: "comments.moderate", run: async (ctx, _a, b) => {
    const levelId = safeInt(b.levelID, 0), target = safeInt(b.target, 0);
    const rows = await ctx.sql`
      select c.comment_id::int as id, 'level' as kind, c.level_id::int as level_id, a.username, c.account_id::int as author, c.body, c.likes, c.created_at
      from geobr_gdps.comments c join geobr_gdps.accounts a on a.account_id = c.account_id
      where not c.deleted and (${levelId} = 0 or c.level_id = ${levelId}) and (${target} = 0 or c.account_id = ${target})
      union all
      select c.comment_id::int, 'account', 0, a.username, c.account_id::int, c.body, c.likes, c.created_at
      from geobr_gdps.account_comments c join geobr_gdps.accounts a on a.account_id = c.account_id
      where not c.deleted and ${levelId} = 0 and (${target} = 0 or c.account_id = ${target})
      order by created_at desc limit 40`;
    return ok({ comments: rows });
  } },
  "comments.delete": { perm: "comments.moderate", run: async (ctx, actor, b) => {
    const id = safeInt(b.commentID, 0);
    const account = b.kind === "account";
    const res = account
      ? await ctx.sql`update geobr_gdps.account_comments set deleted = true where comment_id = ${id} and not deleted returning account_id::int`
      : await ctx.sql`update geobr_gdps.comments set deleted = true where comment_id = ${id} and not deleted returning account_id::int`;
    if (!res.length) return fail("Comentario nao encontrado.");
    await audit(ctx, actor.id, account ? "account_comment.delete" : "comment.delete", account ? "account_comment" : "comment", id, { author: res[0].account_id });
    return ok();
  } },

  // -------------------------------------------------- daily/weekly/event
  "events.list": { perm: "events.manage", run: async (ctx) => {
    const rows = await ctx.sql`
      select t.timely_id::int as id, t.kind, t.level_id::int as level_id, l.name, t.starts_at, t.starts_at <= now() as started
      from geobr_gdps.timely t join geobr_gdps.levels l using (level_id) order by t.starts_at desc limit 60`;
    return ok({ events: rows });
  } },
  "events.set": { perm: "events.manage", run: async (ctx, actor, b) => {
    const kind = safeInt(b.kind, 0, 0, 2);
    const levelId = safeInt(b.levelID, 0);
    const [l] = await ctx.sql`select stars from geobr_gdps.levels where level_id = ${levelId} and not deleted and not blocked`;
    if (!l) return fail("Level nao encontrado.");
    if (kind === 2) {
      // Event: stays until another event level is set.
      const rewards = parseRewards(b.rewards);
      if (!rewards) return fail("Informe as recompensas, ex.: 500 diamantes 1000 orbs");
      const [row] = await ctx.sql`
        insert into geobr_gdps.timely (kind, level_id, starts_at, created_by, rewards)
        values (2, ${levelId}, now(), ${actor.id}, ${rewards}) returning timely_id::int, starts_at`;
      await audit(ctx, actor.id, "event.set", "level", levelId, { kind, timely: row.timely_id, rewards });
      return ok({ id: row.timely_id, starts_at: row.starts_at, rewards });
    }
    // Daily / Weekly: a queue, one level per day (UTC) / week (Monday). Today's
    // (this week's) slot if free, otherwise the first free one after the queue.
    const unit = kind === 0 ? "day" : "week";
    const [dup] = await ctx.sql`select 1 from geobr_gdps.timely where kind = ${kind} and level_id = ${levelId} and starts_at >= date_trunc(${unit}, now())`;
    if (dup) return fail("Esse level ja esta na fila.");
    const row = await ctx.sql.begin(async (tx) => {
      await tx`select pg_advisory_xact_lock(hashtext('gdps-timely-' || ${kind}))`;
      const [slot] = await tx`
        select case when max(starts_at) is null or max(starts_at) < date_trunc(${unit}, now()) then now()
          else date_trunc(${unit}, max(starts_at)) + make_interval(days => ${kind === 0 ? 1 : 7}) end as at
        from geobr_gdps.timely where kind = ${kind}`;
      const [r] = await tx`insert into geobr_gdps.timely (kind, level_id, starts_at, created_by)
        values (${kind}, ${levelId}, ${slot.at}, ${actor.id}) returning timely_id::int, starts_at, starts_at > now() as queued`;
      return r;
    });
    await audit(ctx, actor.id, "event.set", "level", levelId, { kind, timely: row.timely_id, starts_at: row.starts_at });
    return ok({ id: row.timely_id, starts_at: row.starts_at, queued: row.queued });
  } },
  "events.remove": { perm: "events.manage", run: async (ctx, actor, b) => {
    const id = safeInt(b.id, 0);
    const [row] = await ctx.sql`delete from geobr_gdps.timely where timely_id = ${id} returning kind, level_id::int`;
    if (!row) return fail("Evento nao encontrado.");
    await audit(ctx, actor.id, "event.remove", "timely", id, row);
    return ok();
  } },

  // ----------------------------------------------------------- demon list
  "demonlist.set": { perm: "demonlist.manage", run: async (ctx, actor, b) => {
    const levelId = safeInt(b.levelID, 0);
    const [l] = await ctx.sql`select demon from geobr_gdps.levels where level_id = ${levelId} and not deleted`;
    if (!l) return fail("Level nao encontrado.");
    if (!l.demon) return fail("So levels Demon entram na Demon List.");
    const verifier = clean(b.verifier, 30);
    await ctx.sql.begin(async (tx) => {
      await tx`delete from geobr_gdps.demon_list where level_id = ${levelId}`;
      const [n] = await tx`select count(*)::int as n from geobr_gdps.demon_list`;
      const position = safeInt(b.position, n.n + 1, 1, n.n + 1);
      await tx`update geobr_gdps.demon_list set position = position + 1 where position >= ${position}`;
      await tx`insert into geobr_gdps.demon_list (level_id, position, verifier, added_by) values (${levelId}, ${position}, ${verifier}, ${actor.id})`;
      await tx`with ordered as (select level_id, row_number() over (order by position, added_at) as p from geobr_gdps.demon_list)
        update geobr_gdps.demon_list d set position = o.p from ordered o where o.level_id = d.level_id`;
    });
    await audit(ctx, actor.id, "demonlist.set", "level", levelId, { position: b.position, verifier });
    return ok();
  } },
  "demonlist.remove": { perm: "demonlist.manage", run: async (ctx, actor, b) => {
    const levelId = safeInt(b.levelID, 0);
    const [row] = await ctx.sql`delete from geobr_gdps.demon_list where level_id = ${levelId} returning position`;
    if (!row) return fail("Esse level nao esta na lista.");
    await ctx.sql`update geobr_gdps.demon_list set position = position - 1 where position > ${row.position}`;
    await audit(ctx, actor.id, "demonlist.remove", "level", levelId, { position: row.position });
    return ok();
  } },

  // ---------------------------------------------------------------- songs
  "songs.list": { perm: "songs.moderate", run: async (ctx, _a, b) => {
    const status = ["pending", "approved", "blocked", "removed"].includes(String(b.status)) ? String(b.status) : "pending";
    const rows = await ctx.sql`
      select s.song_id::int as id, s.name, s.artist, a.username as uploader, s.size_bytes::int as size, s.status, s.created_at, s.object_path
      from geobr_gdps.songs s left join geobr_gdps.accounts a on a.account_id = s.uploader
      where s.status = ${status} order by s.song_id desc limit 40`;
    return ok({ songs: rows.map((s) => ({ ...s, url: ctx.store.publicUrl(ctx.env.songsBucket, s.object_path), object_path: undefined })) });
  } },
  "songs.review": { perm: "songs.moderate", run: async (ctx, actor, b) => {
    const id = safeInt(b.songID, 0);
    const status = String(b.status);
    if (!["approved", "blocked", "removed", "pending"].includes(status)) return fail("Status invalido.");
    const [s] = await ctx.sql`select status, object_path from geobr_gdps.songs where song_id = ${id} and status <> 'uploading'`;
    if (!s) return fail("Musica nao encontrada.");
    if (s.status === "removed") return fail("Essa musica foi removida (o ID nao e reutilizado).");
    await ctx.sql`update geobr_gdps.songs set status = ${status}, reviewed_by = ${actor.id},
      published_at = case when ${status} = 'approved' then coalesce(published_at, now()) else published_at end where song_id = ${id}`;
    if (status === "removed") await ctx.store.remove(ctx.env.songsBucket, s.object_path);
    await audit(ctx, actor.id, `song.${status}`, "song", id, { from: s.status });
    return ok();
  } },
  "songs.edit": { perm: "songs.moderate", run: async (ctx, actor, b) => {
    const id = safeInt(b.songID, 0);
    const name = clean(b.name, 60).trim(), artist = clean(b.artist, 40).trim();
    if (!name || !artist) return fail("Informe nome e artista.");
    const [before] = await ctx.sql`select name, artist from geobr_gdps.songs where song_id = ${id} and status <> 'removed'`;
    if (!before) return fail("Musica nao encontrada.");
    await ctx.sql`update geobr_gdps.songs set name = ${name}, artist = ${artist} where song_id = ${id}`;
    await audit(ctx, actor.id, "song.edit", "song", id, { before, name, artist });
    return ok();
  } },


  // ------------------------------------------------------- comment mutes
  "accounts.mute": { perm: "comments.moderate", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0);
    const why = await outranks(ctx, actor, id);
    if (why) return fail(why);
    const hours = safeInt(b.hours, 24, 1, 24 * 365);
    const reason = clean(b.reason, 100);
    await ctx.sql`update geobr_gdps.accounts set comment_ban_until = now() + make_interval(hours => ${hours}), comment_ban_reason = ${reason} where account_id = ${id}`;
    await audit(ctx, actor.id, "account.mute", "account", id, { hours, reason });
    return ok();
  } },
  "accounts.unmute": { perm: "comments.moderate", run: async (ctx, actor, b) => {
    const id = safeInt(b.target, 0);
    await ctx.sql`update geobr_gdps.accounts set comment_ban_until = null, comment_ban_reason = null where account_id = ${id}`;
    await audit(ctx, actor.id, "account.unmute", "account", id);
    return ok();
  } },

  // -------------------------------------------------- map packs / gauntlets
  "packs.list": { perm: "admin.panel", run: async (ctx) => {
    const packs = await ctx.sql`select pack_id::int as id, name, level_ids, stars, coins, difficulty, text_color, bar_color, position from geobr_gdps.map_packs order by position, pack_id`;
    const gauntlets = await ctx.sql`select gauntlet_id as id, level_ids from geobr_gdps.gauntlets order by gauntlet_id`;
    return ok({ packs, gauntlets });
  } },
  "packs.save": { perm: "packs.manage", run: async (ctx, actor, b) => {
    const levels = await existingLevels(ctx, b.levels, 20);
    if (!levels.length) return fail("Informe IDs de levels que existem.");
    const fields = {
      name: clean(b.name, 40).trim() || "Map Pack",
      level_ids: levels.join(","),
      stars: safeInt(b.stars, 0, 0, 50),
      coins: safeInt(b.coins, 0, 0, 10),
      difficulty: safeInt(b.difficulty, 0, 0, 10),
      text_color: rgbText(b.text_color),
      bar_color: rgbText(b.bar_color ?? b.text_color),
      position: safeInt(b.position, 100, 0, 10000),
    };
    const packId = safeInt(b.pack_id, 0);
    if (packId > 0) {
      const res = await ctx.sql`update geobr_gdps.map_packs set ${ctx.sql(fields)} where pack_id = ${packId}`;
      if (!res.count) return fail("Map Pack nao encontrado.");
      await audit(ctx, actor.id, "pack.update", "pack", packId, fields);
      return ok({ id: packId });
    }
    const [row] = await ctx.sql`insert into geobr_gdps.map_packs ${ctx.sql({ ...fields, created_by: actor.id })} returning pack_id::int`;
    await audit(ctx, actor.id, "pack.create", "pack", row.pack_id, fields);
    return ok({ id: row.pack_id });
  } },
  "packs.delete": { perm: "packs.manage", run: async (ctx, actor, b) => {
    const id = safeInt(b.pack_id, 0);
    const [row] = await ctx.sql`delete from geobr_gdps.map_packs where pack_id = ${id} returning name`;
    if (!row) return fail("Map Pack nao encontrado.");
    await audit(ctx, actor.id, "pack.delete", "pack", id, row);
    return ok();
  } },
  "gauntlets.save": { perm: "packs.manage", run: async (ctx, actor, b) => {
    const id = safeInt(b.gauntlet_id, 0, 0, 99);
    if (id < 1) return fail("Escolha o tipo da Gauntlet (1 a 99).");
    const levels = await existingLevels(ctx, b.levels, 5);
    if (levels.length !== 5) return fail("A Gauntlet precisa de exatamente 5 levels existentes.");
    await ctx.sql`insert into geobr_gdps.gauntlets (gauntlet_id, level_ids, updated_by) values (${id}, ${levels.join(",")}, ${actor.id})
      on conflict (gauntlet_id) do update set level_ids = excluded.level_ids, updated_by = excluded.updated_by, updated_at = now()`;
    await audit(ctx, actor.id, "gauntlet.save", "gauntlet", id, { levels });
    return ok();
  } },
  "gauntlets.delete": { perm: "packs.manage", run: async (ctx, actor, b) => {
    const id = safeInt(b.gauntlet_id, 0);
    const [row] = await ctx.sql`delete from geobr_gdps.gauntlets where gauntlet_id = ${id} returning level_ids`;
    if (!row) return fail("Gauntlet nao encontrada.");
    await audit(ctx, actor.id, "gauntlet.delete", "gauntlet", id, row);
    return ok();
  } },

  // ------------------------------------------------------------ level lists
  "lists.search": { perm: "admin.panel", run: async (ctx, _a, b) => {
    const q = String(b.q ?? "").trim().slice(0, 40);
    const asId = /^\d+$/.test(q) ? Number(q) : -1;
    const rows = await ctx.sql`
      select l.list_id::int as id, l.name, a.username as creator, l.level_ids, l.stars, l.featured, l.downloads, l.likes, l.deleted
      from geobr_gdps.level_lists l join geobr_gdps.accounts a on a.account_id = l.account_id
      where ${q === ""} or l.list_id = ${asId} or lower(l.name) like ${"%" + q.toLowerCase().replace(/[%_\\]/g, "") + "%"}
      order by l.list_id desc limit 30`;
    return ok({ lists: rows });
  } },
  "lists.rate": { perm: "levels.rate", run: async (ctx, actor, b) => {
    const id = safeInt(b.listID, 0);
    const stars = safeInt(b.stars, 0, 0, 100), featured = safeInt(b.featured, 0, 0, 100000);
    const needed = safeInt(b.count_for_reward, 0, 0, 100);
    const res = await ctx.sql`update geobr_gdps.level_lists set stars = ${stars}, featured = ${featured}, count_for_reward = ${needed}, updated_at = now() where list_id = ${id} and not deleted`;
    if (!res.count) return fail("Lista nao encontrada.");
    await audit(ctx, actor.id, "list.rate", "list", id, { stars, featured, count_for_reward: needed });
    return ok();
  } },
  "lists.delete": { perm: "levels.delete", run: async (ctx, actor, b) => {
    const id = safeInt(b.listID, 0);
    const res = await ctx.sql`update geobr_gdps.level_lists set deleted = true where list_id = ${id} and not deleted`;
    if (!res.count) return fail("Lista nao encontrada.");
    await audit(ctx, actor.id, "list.delete", "list", id);
    return ok();
  } },

  // ---------------------------------------------------------------- audit
  "audit.list": { perm: "audit.view", run: async (ctx, _a, b) => {
    const page = safeInt(b.page, 0, 0, 1000);
    const rows = await ctx.sql`
      select l.id::int, l.action, l.target_type, l.target_id, l.details, l.at, a.username as actor
      from geobr_gdps.audit_log l left join geobr_gdps.accounts a on a.account_id = l.actor
      order by l.id desc limit 30 offset ${page * 30}`;
    return ok({ entries: rows });
  } },
};

export const ADMIN_ACTIONS = Object.keys(actions);

/** Runs an admin action for an already authenticated account (panel, in-game tools, comment commands). */
export async function runAction(ctx: Ctx, accountId: number, name: string, body: Json) {
  const action = actions[name];
  if (!action) return fail("Acao desconhecida.");
  const g = await grantsOf(ctx, accountId);
  const needed = Array.isArray(action.perm) ? action.perm : [action.perm];
  if (!needed.some((p) => can(g, p))) return fail("Sem permissao para essa acao.");
  return await action.run(ctx, { id: accountId, g }, body);
}

export async function admin(ctx: Ctx) {
  const account = await jsonAccount(ctx);
  if (!account) return fail("Conta ou senha incorreta.");
  return await runAction(ctx, account.account_id, String(ctx.json.action ?? ""), ctx.json);
}
