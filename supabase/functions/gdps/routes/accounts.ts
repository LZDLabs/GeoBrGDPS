// Accounts (no e-mail), login, cloud save, profiles, user search and the
// global leaderboards.
import { type Ctx, attempt, authenticate, badges, failures, gdAccount, grantsOf, can, intParam, param } from "../lib/core.ts";
import { age, b64urlDecode, clean, gjp2Of, idList, record, safeInt } from "../lib/gd.ts";

const USERNAME = /^[A-Za-z0-9]{3,20}$/;
const PASSWORD = /^[\x21-\x7e]{6,20}$/;
const MAX_SAVE_BYTES = 50 * 1024 * 1024;

export async function hashSecret(ctx: Ctx, gjp2: string): Promise<string> {
  const [row] = await ctx.sql`select extensions.crypt(${gjp2}, extensions.gen_salt('bf', 8)) as h`;
  return row.h;
}

export function validPassword(password: string): number {
  if (password.length < 6) return -8;
  if (!PASSWORD.test(password)) return -5;
  return 0;
}

export async function registerGJAccount(ctx: Ctx): Promise<string> {
  const userName = param(ctx, "userName").trim();
  const password = param(ctx, "password");
  if (userName.length < 3) return "-9";
  if (!USERNAME.test(userName)) return "-4";
  const bad = validPassword(password);
  if (bad) return String(bad);
  const [recent] = await ctx.sql`
    select count(*)::int as n from geobr_gdps.auth_attempts
    where kind = 'register' and subject = ${ctx.ip} and ok and at > now() - interval '1 hour'`;
  if ((recent?.n ?? 0) >= 5) return "-1";
  const [taken] = await ctx.sql`select 1 from geobr_gdps.accounts where lower(username) = lower(${userName})`;
  if (taken) return "-2";
  const hash = await hashSecret(ctx, await gjp2Of(password));
  try {
    await ctx.sql`insert into geobr_gdps.accounts (username, gjp2_hash) values (${userName}, ${hash})`;
  } catch (e) {
    if (String((e as { code?: string }).code) === "23505") return "-2";
    throw e;
  }
  await attempt(ctx, "register", ctx.ip, true);
  return "1";
}

export async function loginGJAccount(ctx: Ctx): Promise<string> {
  const userName = param(ctx, "userName").trim();
  let gjp2 = param(ctx, "gjp2");
  if (!gjp2 && ctx.params.has("password")) gjp2 = await gjp2Of(param(ctx, "password"));
  if (!USERNAME.test(userName)) return "-1";
  const subject = userName.toLowerCase();
  if (await failures(ctx, "login", subject, 15) >= 10) return "-1";
  const [row] = await ctx.sql`select account_id::int from geobr_gdps.accounts where lower(username) = ${subject}`;
  if (!row) { await attempt(ctx, "login", subject, false); return "-1"; }
  const account = await authenticate(ctx, row.account_id, gjp2, true);
  if (!account) { await attempt(ctx, "login", subject, false); return "-1"; }
  if (account.is_banned) return "-12";
  await ctx.sql`update geobr_gdps.accounts set last_login = now() where account_id = ${account.account_id}`;
  await attempt(ctx, "login", subject, true);
  return `${account.account_id},${account.account_id}`;
}

export function getAccountURL(ctx: Ctx): string {
  return ctx.env.functionUrl;
}

export async function backupGJAccountNew(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const data = param(ctx, "saveData");
  if (!data) return "-1";
  const bytes = new TextEncoder().encode(data);
  if (bytes.length > MAX_SAVE_BYTES) return "-4";
  const path = `${account.account_id}/save.dat`;
  await ctx.store.put(ctx.env.savesBucket, path, bytes, "text/plain");
  const gameVersion = safeInt(param(ctx, "gameVersion"), 22, 0, 100);
  const binaryVersion = safeInt(param(ctx, "binaryVersion"), 42, 0, 1000);
  await ctx.sql`
    insert into geobr_gdps.saves (account_id, object_path, size_bytes, game_version, binary_version, updated_at)
    values (${account.account_id}, ${path}, ${bytes.length}, ${gameVersion}, ${binaryVersion}, now())
    on conflict (account_id) do update set object_path = excluded.object_path, size_bytes = excluded.size_bytes,
      game_version = excluded.game_version, binary_version = excluded.binary_version, updated_at = now()`;
  return "1";
}

export async function syncGJAccountNew(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const [row] = await ctx.sql`select object_path, game_version, binary_version from geobr_gdps.saves where account_id = ${account.account_id}`;
  if (!row) return "-1";
  const bytes = await ctx.store.get(ctx.env.savesBucket, row.object_path);
  if (!bytes) return "-1";
  return `${new TextDecoder().decode(bytes)};${row.game_version};${row.binary_version};a;a`;
}

export async function updateGJAccSettings20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  await ctx.sql`
    update geobr_gdps.accounts set
      msg_state = ${safeInt(param(ctx, "mS"), 0, 0, 2)},
      friend_state = ${safeInt(param(ctx, "frS"), 0, 0, 1)},
      comment_state = ${safeInt(param(ctx, "cS"), 0, 0, 2)},
      youtube = ${clean(param(ctx, "yt"), 40)},
      twitter = ${clean(param(ctx, "twitter"), 40)},
      twitch = ${clean(param(ctx, "twitch"), 40)}
    where account_id = ${account.account_id}`;
  return "1";
}

/** Creator points: rated +1, featured +1, epic/legendary/mythic +1 each tier. */
export async function recomputeCreatorPoints(ctx: Ctx, accountId: number) {
  await ctx.sql`
    update geobr_gdps.accounts set creator_points = coalesce((
      select sum((stars > 0)::int + (feature_score > 0)::int + (epic >= 1)::int + (epic >= 2)::int + (epic >= 3)::int)
      from geobr_gdps.levels where account_id = ${accountId} and not deleted and not blocked), 0)
    where account_id = ${accountId}`;
}

function numbers(text: string, count: number): number[] {
  const parts = text.split(",").map((x) => safeInt(x, 0, 0, 100000));
  while (parts.length < count) parts.push(0);
  return parts.slice(0, count);
}

export async function updateGJUserScore22(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const p = (name: string, max = 1_000_000) => safeInt(param(ctx, name), 0, 0, max);
  const stats = {
    stars: p("stars"), moons: p("moons"), demons: p("demons", 100000), diamonds: p("diamonds", 10_000_000),
    coins: p("coins", 1000), user_coins: p("userCoins", 1_000_000),
  };
  // Demons by difficulty from the demon IDs the client reports (GeoBR levels only).
  const demonIds = idList(param(ctx, "dinfo"), 5000);
  let counts = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  if (demonIds.length) {
    const rows = await ctx.sql`
      select demon_diff, (length = 5) as plat, count(*)::int as n from geobr_gdps.levels
      where level_id = any(string_to_array(${demonIds.join(",")}, ',')::bigint[]) and demon
      group by 1, 2`;
    const slot: Record<number, number> = { 3: 0, 4: 1, 0: 2, 5: 3, 6: 4 };
    counts = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    for (const r of rows) counts[(slot[r.demon_diff] ?? 2) + (r.plat ? 5 : 0)] += r.n;
  }
  const weekly = idList(param(ctx, "dinfow"), 5000).length;
  const gauntlet = idList(param(ctx, "dinfog"), 5000).length;
  const sinfo = numbers(param(ctx, "sinfo"), 12);
  const demonsInfo = [...counts, weekly, gauntlet].join(",");
  const levelsInfo = [...sinfo.slice(0, 6), safeInt(param(ctx, "sinfod"), 0, 0, 100000), safeInt(param(ctx, "sinfog"), 0, 0, 100000)].join(",");
  const platInfo = sinfo.slice(6, 12).join(",");

  const [before] = await ctx.sql`select stars, moons, demons, diamonds, coins, user_coins, stats_updated_at from geobr_gdps.accounts where account_id = ${account.account_id}`;
  const jump = (k: keyof typeof stats, limit: number) => stats[k] - (before?.[k] ?? 0) > limit;
  const flagged = jump("stars", 2000) || jump("moons", 2000) || jump("demons", 100) || jump("user_coins", 200) || jump("coins", 60);

  await ctx.sql`
    update geobr_gdps.accounts set
      stars = ${stats.stars}, moons = ${stats.moons}, demons = ${stats.demons}, diamonds = ${stats.diamonds},
      coins = ${stats.coins}, user_coins = ${stats.user_coins},
      icon = ${p("icon", 9999)}, icon_type = ${p("iconType", 20)},
      color1 = ${p("color1", 1000)}, color2 = ${p("color2", 1000)}, color3 = ${safeInt(param(ctx, "color3"), -1, -1, 1000)},
      special = ${p("special", 1000)},
      acc_icon = ${p("accIcon", 9999)}, acc_ship = ${p("accShip", 9999)}, acc_ball = ${p("accBall", 9999)},
      acc_bird = ${p("accBird", 9999)}, acc_dart = ${p("accDart", 9999)}, acc_robot = ${p("accRobot", 9999)},
      acc_spider = ${p("accSpider", 9999)}, acc_swing = ${p("accSwing", 9999)}, acc_jetpack = ${p("accJetpack", 9999)},
      acc_explosion = ${p("accExplosion", 9999)}, acc_glow = ${p("accGlow", 1)},
      demons_info = ${demonsInfo}, levels_info = ${levelsInfo}, platformer_info = ${platInfo},
      stats_updated_at = now()
    where account_id = ${account.account_id}`;
  const changed = !before || (Object.keys(stats) as Array<keyof typeof stats>).some((k) => before[k] !== stats[k]);
  if (changed) {
    await ctx.sql`
      insert into geobr_gdps.stat_history (account_id, stars, moons, demons, diamonds, coins, user_coins, flagged)
      values (${account.account_id}, ${stats.stars}, ${stats.moons}, ${stats.demons}, ${stats.diamonds}, ${stats.coins}, ${stats.user_coins}, ${flagged})`;
  }
  return String(account.account_id);
}

async function globalRank(ctx: Ctx, accountId: number): Promise<number> {
  const [row] = await ctx.sql`
    select case when a.lb_banned or a.is_banned or a.stars = 0 then 0 else (
      select count(*)::int + 1 from geobr_gdps.accounts b
      where not b.lb_banned and not b.is_banned and (b.stars > a.stars or (b.stars = a.stars and b.account_id < a.account_id))) end as rank
    from geobr_gdps.accounts a where a.account_id = ${accountId}`;
  return row?.rank ?? 0;
}

export async function getGJUserInfo20(ctx: Ctx): Promise<string> {
  const target = intParam(ctx, "targetAccountID");
  const viewer = await gdAccount(ctx);
  const [a] = await ctx.sql`select *, account_id::int as id from geobr_gdps.accounts where account_id = ${target}`;
  if (!a) return "-1";
  if (viewer && viewer.account_id !== target) {
    const [blocked] = await ctx.sql`select 1 from geobr_gdps.blocks where owner = ${target} and target = ${viewer.account_id}`;
    if (blocked) return "-1";
  }
  const badge = (await badges(ctx, [target])).get(target)?.badge ?? 0;
  let friendState = 0;
  const extra: Array<[number, unknown]> = [];
  if (viewer && viewer.account_id !== target) {
    const [lo, hi] = viewer.account_id < target ? [viewer.account_id, target] : [target, viewer.account_id];
    const [friend] = await ctx.sql`select 1 from geobr_gdps.friendships where a = ${lo} and b = ${hi}`;
    if (friend) friendState = 1;
    else {
      const [incoming] = await ctx.sql`select request_id::int, body, created_at from geobr_gdps.friend_requests where from_account = ${target} and to_account = ${viewer.account_id}`;
      const [outgoing] = await ctx.sql`select 1 from geobr_gdps.friend_requests where from_account = ${viewer.account_id} and to_account = ${target}`;
      if (incoming) {
        friendState = 3;
        extra.push([32, incoming.request_id], [35, incoming.body], [37, age(incoming.created_at, ctx.now)]);
      } else if (outgoing) friendState = 4;
    }
  }
  const pairs: Array<[number, unknown]> = [
    [1, a.username], [2, a.id], [13, a.coins], [17, a.user_coins], [10, a.color1], [11, a.color2], [51, a.color3],
    [3, a.stars], [46, a.diamonds], [52, a.moons], [4, a.demons], [8, a.creator_points],
    [18, a.msg_state], [19, a.friend_state], [50, a.comment_state], [20, a.youtube],
    [21, a.acc_icon], [22, a.acc_ship], [23, a.acc_ball], [24, a.acc_bird], [25, a.acc_dart], [26, a.acc_robot],
    [28, a.acc_glow], [43, a.acc_spider], [48, a.acc_explosion], [53, a.acc_swing], [54, a.acc_jetpack],
    [30, await globalRank(ctx, target)], [16, a.id], [31, friendState], [44, a.twitter], [45, a.twitch], [29, 1],
    [49, badge], [55, a.demons_info], [56, a.levels_info], [57, a.platformer_info],
    ...extra,
  ];
  if (viewer && viewer.account_id === target) {
    const [counts] = await ctx.sql`select
      (select count(*)::int from geobr_gdps.messages where to_account = ${target} and not is_read and not deleted_by_receiver) as messages,
      (select count(*)::int from geobr_gdps.friend_requests where to_account = ${target} and is_new) as requests,
      (select count(*)::int from geobr_gdps.friendships where (a = ${target} and new_for_a) or (b = ${target} and new_for_b)) as friends`;
    pairs.push([38, counts.messages], [39, counts.requests], [40, counts.friends]);
  }
  return record(pairs);
}

export async function getGJUsers20(ctx: Ctx): Promise<string> {
  const str = param(ctx, "str").trim().slice(0, 20);
  const page = safeInt(param(ctx, "page"), 0, 0, 1000);
  if (!str) return "-1";
  const asId = /^\d+$/.test(str) ? Number(str) : -1;
  const rows = await ctx.sql`
    select account_id::int as id, username, coins, user_coins, icon, color1, color2, color3, icon_type, special, stars, creator_points, demons, moons,
      count(*) over ()::int as total
    from geobr_gdps.accounts
    where not is_banned and (lower(username) like ${str.toLowerCase().replace(/[%_\\]/g, "") + "%"} or account_id = ${asId})
    order by (lower(username) = ${str.toLowerCase()}) desc, stars desc, account_id
    limit 10 offset ${page * 10}`;
  if (!rows.length) return "-1";
  const users = rows.map((u) => record([[1, u.username], [2, u.id], [13, u.coins], [17, u.user_coins], [9, u.icon], [10, u.color1], [11, u.color2], [51, u.color3], [14, u.icon_type], [15, u.special], [16, u.id], [3, u.stars], [8, u.creator_points], [4, u.demons], [52, u.moons]]));
  return `${users.join("|")}#${rows[0].total}:${page * 10}:10`;
}

export async function getGJScores20(ctx: Ctx): Promise<string> {
  const type = param(ctx, "type", "top");
  const count = safeInt(param(ctx, "count"), 100, 1, 200);
  const viewer = await gdAccount(ctx);
  let rows;
  if (type === "creators") {
    rows = await ctx.sql`
      select *, account_id::int as id, row_number() over (order by creator_points desc, account_id)::int as rank
      from geobr_gdps.accounts where creator_points > 0 and not is_banned and not lb_banned
      order by creator_points desc, account_id limit ${count}`;
  } else if (type === "friends" && viewer) {
    rows = await ctx.sql`
      select *, account_id::int as id, row_number() over (order by stars desc, account_id)::int as rank
      from geobr_gdps.accounts where not is_banned and (account_id = ${viewer.account_id} or account_id in (
        select b from geobr_gdps.friendships where a = ${viewer.account_id} union select a from geobr_gdps.friendships where b = ${viewer.account_id}))
      order by stars desc, account_id limit ${count}`;
  } else {
    const ranked = await ctx.sql`
      select *, account_id::int as id, row_number() over (order by stars desc, account_id)::int as rank
      from geobr_gdps.accounts where not is_banned and not lb_banned and stars > 0
      order by stars desc, account_id`;
    if (type === "relative" && viewer) {
      const me = ranked.findIndex((r) => r.id === viewer.account_id);
      const start = Math.max(0, (me < 0 ? ranked.length : me) - 12);
      rows = ranked.slice(start, start + 25);
    } else rows = ranked.slice(0, count);
  }
  if (!rows.length) return "-1";
  return rows.map((u) => record([[1, u.username], [2, u.id], [13, u.coins], [17, u.user_coins], [6, u.rank], [9, u.icon], [10, u.color1], [11, u.color2], [51, u.color3], [14, u.icon_type], [15, u.special], [16, u.id], [3, u.stars], [8, u.creator_points], [4, u.demons], [7, u.id], [46, u.diamonds], [52, u.moons]])).join("|");
}

export async function requestUserAccess(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const g = await grantsOf(ctx, account.account_id);
  if (can(g, "mod.elder")) return "2";
  if (can(g, "mod.badge") || can(g, "levels.rate") || can(g, "levels.suggest")) return "1";
  return "-1";
}

/** Decoded comment text of a friend request etc. is kept base64 as GD sends it. */
export const decodeText = b64urlDecode;
