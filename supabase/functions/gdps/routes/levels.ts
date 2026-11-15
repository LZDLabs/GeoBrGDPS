// Levels: upload/update, search lists, download, rating and moderation from
// GD's own buttons, Daily/Weekly/Event and the per-level leaderboards.
import { type Ctx, audit, can, gdAccount, grantsOf, intParam, param } from "../lib/core.ts";
import { DEMON_KEY43, SALT_CHESTS, XOR_CHESTS, age, b64urlEncode, clean, decodeChk, difficultyOfStars, encodeLevelPassword, genMulti, genSolo, genSolo2, idList, record, safeInt, secondsToReset, sha1, xorCipher } from "../lib/gd.ts";
import { recomputeCreatorPoints } from "./accounts.ts";
import { songsSection } from "./songs.ts";
import { listLevelIds } from "./packs.ts";

const MAX_LEVEL_BYTES = 16 * 1024 * 1024;
const TIMELY_OFFSET = [0, 100001, 200001];

// deno-lint-ignore no-explicit-any
type Row = Record<string, any>;

const levelColumns = (ctx: Ctx) => ctx.sql`
  l.level_id::int as id, l.account_id::int as owner, a.username, l.name, l.description, l.version, l.length, l.audio_track,
  l.song_id::int as song_id, l.song_ids, l.sfx_ids, l.game_version, l.objects, l.coins, l.requested_stars, l.two_player,
  l.original::int as original, l.password, l.unlisted, l.ldm, l.wt, l.wt2, l.ts, l.extra_string, l.is_auto, l.stars,
  l.difficulty, l.demon, l.demon_diff, l.feature_score, l.epic, l.coins_verified, l.downloads, l.likes, l.created_at, l.updated_at`;

function listRecord(l: Row): string {
  return record([
    [1, l.id], [2, l.name], [5, l.version], [6, l.owner], [8, 10], [9, l.difficulty], [10, l.downloads],
    [12, l.audio_track], [13, l.game_version], [14, l.likes], [17, l.demon ? 1 : 0], [43, l.demon_diff],
    [25, l.stars === 1 || (l.stars === 0 && l.is_auto) ? 1 : 0], [18, l.stars], [19, l.feature_score], [42, l.epic],
    [45, l.objects], [3, l.description], [15, l.length], [30, l.original], [31, l.two_player ? 1 : 0],
    [37, l.coins], [38, l.coins_verified ? 1 : 0], [39, l.requested_stars], [46, l.wt], [47, l.wt2],
    [40, l.ldm ? 1 : 0], [35, l.song_id], [57, l.ts],
  ]);
}

async function listResponse(ctx: Ctx, rows: Row[], total: number, offset: number): Promise<string> {
  if (!rows.length) return "-1";
  const creators = new Map<number, string>();
  for (const l of rows) creators.set(l.owner, `${l.owner}:${clean(l.username)}:${l.owner}`);
  const songs = await songsSection(ctx, rows.map((l) => l.song_id));
  const hash = await genMulti(rows.map((l) => ({ id: l.id, stars: l.stars, coinsVerified: l.coins_verified })));
  return `${rows.map(listRecord).join("|")}#${[...creators.values()].join("|")}#${songs}#${total}:${offset}:10#${hash}`;
}

export async function getGJLevels21(ctx: Ctx): Promise<string> {
  const sql = ctx.sql;
  const type = intParam(ctx, "type", 0);
  const str = param(ctx, "str").trim();
  const page = safeInt(param(ctx, "page"), 0, 0, 5000);
  const offset = page * 10;
  const viewer = await gdAccount(ctx);
  const conds = [sql`not l.deleted`, sql`not l.blocked`];
  let order = sql`l.level_id desc`;
  let publicOnly = true;

  switch (type) {
    case 0: case 15:
      if (/^\d+$/.test(str)) { conds.push(sql`l.level_id = ${Number(str)}`); publicOnly = false; }
      else if (str) conds.push(sql`lower(l.name) like ${"%" + str.toLowerCase().replace(/[%_\\]/g, "").slice(0, 20) + "%"}`);
      order = sql`l.likes desc, l.level_id desc`;
      break;
    case 1: order = sql`l.downloads desc, l.level_id desc`; break;
    case 2: order = sql`l.likes desc, l.level_id desc`; break;
    case 3: conds.push(sql`l.created_at > now() - interval '7 days'`); order = sql`l.likes desc, l.level_id desc`; break;
    case 4: break;
    case 5: {
      const owner = safeInt(str, 0);
      conds.push(sql`l.account_id = ${owner}`);
      if (viewer && viewer.account_id === owner) publicOnly = false;
      break;
    }
    case 6: case 17: conds.push(sql`l.feature_score > 0`); order = sql`l.rated_at desc nulls last, l.level_id desc`; break;
    case 7: conds.push(sql`l.stars = 0 and l.length >= 2 and l.objects >= 1000`); break;
    case 10: case 19: {
      const ids = idList(str, 100);
      if (!ids.length) return "-1";
      conds.push(sql`l.level_id = any(string_to_array(${ids.join(",")}, ',')::bigint[])`);
      publicOnly = false;
      break;
    }
    case 11: conds.push(sql`l.stars > 0`); order = sql`l.rated_at desc nulls last, l.level_id desc`; break;
    case 12: {
      const ids = idList(param(ctx, "followed"), 500);
      if (!ids.length) return "-1";
      conds.push(sql`l.account_id = any(string_to_array(${ids.join(",")}, ',')::bigint[])`);
      break;
    }
    case 13: {
      if (!viewer) return "-1";
      conds.push(sql`l.account_id in (select b from geobr_gdps.friendships where a = ${viewer.account_id} union select a from geobr_gdps.friendships where b = ${viewer.account_id})`);
      break;
    }
    case 16: conds.push(sql`l.epic > 0`); order = sql`l.rated_at desc nulls last, l.level_id desc`; break;
    case 21: case 22: case 23: {
      const kind = type - 21;
      conds.push(sql`l.level_id in (select level_id from geobr_gdps.timely where kind = ${kind} and starts_at <= now())`);
      order = sql`(select max(starts_at) from geobr_gdps.timely t where t.level_id = l.level_id and t.kind = ${kind}) desc`;
      break;
    }
    case 25: {
      const ids = await listLevelIds(ctx, safeInt(str, 0));
      if (!ids || !ids.length) return "-1";
      conds.push(sql`l.level_id = any(string_to_array(${ids.join(",")}, ',')::bigint[])`);
      order = sql`array_position(string_to_array(${ids.join(",")}, ',')::bigint[], l.level_id)`;
      publicOnly = false;
      break;
    }
    case 27:
      conds.push(sql`l.stars = 0 and exists (select 1 from geobr_gdps.suggestions s where s.level_id = l.level_id)`);
      order = sql`(select max(id) from geobr_gdps.suggestions s where s.level_id = l.level_id) desc`;
      break;
    default:
      return "-1";
  }
  if (publicOnly) conds.push(sql`l.unlisted = 0`);

  // Filters from GD's search screen.
  const diff = param(ctx, "diff", "-");
  if (diff && diff !== "-") {
    const parts: ReturnType<typeof sql>[] = [];
    for (const d of diff.split(",").map((x) => Number.parseInt(x, 10))) {
      if (d === -1) parts.push(sql`(l.difficulty = 0 and not l.demon and l.stars <> 1)`);
      else if (d === -3) parts.push(sql`(l.stars = 1 or (l.stars = 0 and l.is_auto))`);
      else if (d === -2) {
        const demon = intParam(ctx, "demonFilter", 0);
        parts.push(demon >= 1 && demon <= 5 ? sql`(l.demon and l.demon_diff = ${DEMON_KEY43[demon]})` : sql`l.demon`);
      } else if (d >= 1 && d <= 5) parts.push(sql`(l.difficulty = ${d * 10} and not l.demon and l.stars <> 1)`);
    }
    if (parts.length) conds.push(sql`(${parts.reduce((acc, p) => sql`${acc} or ${p}`)})`);
  }
  const len = param(ctx, "len", "-");
  if (len && len !== "-") {
    const lens = len.split(",").map((x) => safeInt(x, -1)).filter((x) => x >= 0 && x <= 5);
    if (lens.length) conds.push(sql`l.length = any(string_to_array(${lens.join(",")}, ',')::int[])`);
  }
  const completed = idList(param(ctx, "completedLevels").replace(/[()]/g, ""), 5000);
  if (param(ctx, "uncompleted") === "1" && completed.length) conds.push(sql`not (l.level_id = any(string_to_array(${completed.join(",")}, ',')::bigint[]))`);
  if (param(ctx, "onlyCompleted") === "1") {
    if (!completed.length) return "-1";
    conds.push(sql`l.level_id = any(string_to_array(${completed.join(",")}, ',')::bigint[])`);
  }
  const tiers: ReturnType<typeof sql>[] = [];
  if (param(ctx, "featured") === "1") tiers.push(sql`l.feature_score > 0`);
  if (param(ctx, "epic") === "1") tiers.push(sql`l.epic = 1`);
  if (param(ctx, "legendary") === "1") tiers.push(sql`l.epic = 2`);
  if (param(ctx, "mythic") === "1") tiers.push(sql`l.epic = 3`);
  if (tiers.length) conds.push(sql`(${tiers.reduce((acc, p) => sql`${acc} or ${p}`)})`);
  if (param(ctx, "original") === "1") conds.push(sql`l.original = 0`);
  if (param(ctx, "twoPlayer") === "1") conds.push(sql`l.two_player`);
  if (param(ctx, "coins") === "1") conds.push(sql`l.coins > 0 and l.coins_verified`);
  if (param(ctx, "noStar") === "1") conds.push(sql`l.stars = 0`);
  if (param(ctx, "star") === "1") conds.push(sql`l.stars > 0`);
  if (ctx.params.has("song")) {
    const song = intParam(ctx, "song", 0);
    if (param(ctx, "customSong") === "1" || param(ctx, "customSong") === "true") conds.push(sql`l.song_id = ${song}`);
    else conds.push(sql`l.song_id = 0 and l.audio_track = ${song - 1}`);
  }

  const where = conds.reduce((acc, c) => sql`${acc} and ${c}`);
  const rows = await sql`
    select ${levelColumns(ctx)}, count(*) over ()::int as total
    from geobr_gdps.levels l join geobr_gdps.accounts a on a.account_id = l.account_id
    where ${where} order by ${order} limit 10 offset ${offset}`;
  return listResponse(ctx, rows, rows[0]?.total ?? 0, offset);
}

/** Daily = the queued level of today (UTC), Weekly = of this week (Monday);
 *  none when the queue has nothing for the period. Event = the last one set. */
async function currentTimely(ctx: Ctx, kind: number): Promise<Row | undefined> {
  const since = kind === 0 ? ctx.sql`date_trunc('day', now())` : kind === 1 ? ctx.sql`date_trunc('week', now())` : ctx.sql`'-infinity'::timestamptz`;
  const [row] = await ctx.sql`
    select t.timely_id::int as timely_id, t.level_id::int as level_id, t.rewards from geobr_gdps.timely t
    join geobr_gdps.levels l on l.level_id = t.level_id
    where t.kind = ${kind} and t.starts_at <= now() and t.starts_at >= ${since} and not l.deleted and not l.blocked
    order by t.starts_at desc, t.timely_id desc limit 1`;
  return row;
}

export async function downloadGJLevel22(ctx: Ctx): Promise<string> {
  let levelId = intParam(ctx, "levelID", 0);
  let dailyId = 0;
  if (levelId < 0 && levelId >= -3) {
    const kind = -levelId - 1;
    const timely = await currentTimely(ctx, kind);
    if (!timely) return "-1";
    levelId = timely.level_id;
    dailyId = timely.timely_id + TIMELY_OFFSET[kind];
  }
  if (levelId <= 0) return "-1";
  const [l] = await ctx.sql`
    select ${levelColumns(ctx)}, d.data
    from geobr_gdps.levels l join geobr_gdps.accounts a on a.account_id = l.account_id
    join geobr_gdps.level_data d on d.level_id = l.level_id
    where l.level_id = ${levelId} and not l.deleted and not l.blocked`;
  if (!l) return "-1";
  await ctx.sql`update geobr_gdps.levels set downloads = downloads + 1 where level_id = ${levelId}`;
  const pairs: Array<[number, unknown]> = [
    [1, l.id], [2, l.name], [3, l.description], [4, l.data], [5, l.version], [6, l.owner], [8, 10], [9, l.difficulty],
    [10, l.downloads + 1], [11, 1], [12, l.audio_track], [13, l.game_version], [14, l.likes], [17, l.demon ? 1 : 0],
    [43, l.demon_diff], [25, l.stars === 1 || (l.stars === 0 && l.is_auto) ? 1 : 0], [18, l.stars], [19, l.feature_score],
    [42, l.epic], [45, l.objects], [15, l.length], [30, l.original], [31, l.two_player ? 1 : 0],
    [28, age(l.created_at, ctx.now)], [29, age(l.updated_at, ctx.now)], [35, l.song_id], [36, l.extra_string],
    [37, l.coins], [38, l.coins_verified ? 1 : 0], [39, l.requested_stars], [46, l.wt], [47, l.wt2], [40, l.ldm ? 1 : 0],
    [27, encodeLevelPassword(l.password)], [52, l.song_ids], [53, l.sfx_ids], [57, l.ts],
  ];
  if (dailyId) pairs.push([41, dailyId]);
  const chk = [l.owner, l.stars, l.demon ? 1 : 0, l.id, l.coins_verified ? 1 : 0, l.feature_score, l.password, dailyId].join(",");
  let out = `${record(pairs)}#${await genSolo(l.data)}#${await genSolo2(chk)}`;
  if (dailyId) out += `#${l.owner}:${clean(l.username)}:${l.owner}`;
  return out;
}

export async function uploadGJLevel21(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const data = param(ctx, "levelString");
  if (!data || new TextEncoder().encode(data).length > MAX_LEVEL_BYTES) return "-1";
  const name = clean(param(ctx, "levelName"), 20).trim();
  if (!name) return "-1";
  const fields = {
    name,
    description: param(ctx, "levelDesc").replace(/[^A-Za-z0-9+/=_-]/g, "").slice(0, 400),
    version: safeInt(param(ctx, "levelVersion"), 1, 1, 100000),
    length: safeInt(param(ctx, "levelLength"), 0, 0, 5),
    audio_track: safeInt(param(ctx, "audioTrack"), 0, 0, 1000),
    song_id: safeInt(param(ctx, "songID"), 0, 0),
    song_ids: idList(param(ctx, "songIDs"), 100).join(","),
    sfx_ids: idList(param(ctx, "sfxIDs"), 500).join(","),
    game_version: safeInt(param(ctx, "gameVersion"), 22, 0, 100),
    binary_version: safeInt(param(ctx, "binaryVersion"), 42, 0, 1000),
    objects: safeInt(param(ctx, "objects"), 0, 0),
    coins: safeInt(param(ctx, "coins"), 0, 0, 3),
    requested_stars: safeInt(param(ctx, "requestedStars"), 0, 0, 10),
    two_player: param(ctx, "twoPlayer") === "1",
    original: safeInt(param(ctx, "original"), 0, 0),
    password: /^\d{1,7}$/.test(param(ctx, "password")) ? param(ctx, "password") : "0",
    unlisted: safeInt(param(ctx, "unlisted"), 0, 0, 2),
    ldm: param(ctx, "ldm") === "1",
    wt: safeInt(param(ctx, "wt"), 0, 0),
    wt2: safeInt(param(ctx, "wt2"), 0, 0),
    ts: safeInt(param(ctx, "ts"), 0, 0),
    extra_string: param(ctx, "extraString").slice(0, 2000),
    is_auto: param(ctx, "auto") === "1",
  };
  const requested = intParam(ctx, "levelID", 0);
  if (requested > 0) {
    const [own] = await ctx.sql`select level_id::int from geobr_gdps.levels where level_id = ${requested} and account_id = ${account.account_id} and not deleted`;
    if (own) {
      await ctx.sql.begin(async (tx) => {
        await tx`update geobr_gdps.levels set ${tx(fields)}, updated_at = now() where level_id = ${requested}`;
        await tx`insert into geobr_gdps.level_data (level_id, data) values (${requested}, ${data}) on conflict (level_id) do update set data = excluded.data`;
      });
      return String(requested);
    }
  }
  const [recent] = await ctx.sql`select count(*)::int as n from geobr_gdps.levels where account_id = ${account.account_id} and created_at > now() - interval '1 hour'`;
  if ((recent?.n ?? 0) >= 10) return "-1";
  const id = await ctx.sql.begin(async (tx) => {
    const [row] = await tx`insert into geobr_gdps.levels ${tx({ ...fields, account_id: account.account_id })} returning level_id::int`;
    await tx`insert into geobr_gdps.level_data (level_id, data) values (${row.level_id}, ${data})`;
    return row.level_id as number;
  });
  return String(id);
}

export async function deleteGJLevelUser20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const levelId = intParam(ctx, "levelID");
  const [l] = await ctx.sql`select account_id::int from geobr_gdps.levels where level_id = ${levelId} and not deleted`;
  if (!l) return "-1";
  if (l.account_id !== account.account_id) {
    if (!can(await grantsOf(ctx, account.account_id), "levels.delete")) return "-1";
    await audit(ctx, account.account_id, "level.delete", "level", levelId, { via: "gd" });
  }
  await ctx.sql`update geobr_gdps.levels set deleted = true, updated_at = now() where level_id = ${levelId}`;
  await recomputeCreatorPoints(ctx, l.account_id);
  return "1";
}

export async function updateGJDesc20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const desc = param(ctx, "levelDesc").replace(/[^A-Za-z0-9+/=_-]/g, "").slice(0, 400);
  const res = await ctx.sql`update geobr_gdps.levels set description = ${desc}, updated_at = now() where level_id = ${intParam(ctx, "levelID")} and account_id = ${account.account_id} and not deleted`;
  return res.count ? "1" : "-1";
}

export async function reportGJLevel(ctx: Ctx): Promise<string> {
  const levelId = intParam(ctx, "levelID");
  const [l] = await ctx.sql`select 1 from geobr_gdps.levels where level_id = ${levelId}`;
  if (!l) return "-1";
  const [dup] = await ctx.sql`select 1 from geobr_gdps.level_reports where level_id = ${levelId} and reporter = ${ctx.ip} and at > now() - interval '1 day'`;
  if (!dup) await ctx.sql`insert into geobr_gdps.level_reports (level_id, reporter) values (${levelId}, ${ctx.ip})`;
  return "1";
}

export async function rateGJStars211(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const stars = safeInt(param(ctx, "stars"), 0, 1, 10);
  const levelId = intParam(ctx, "levelID");
  const [l] = await ctx.sql`select 1 from geobr_gdps.levels where level_id = ${levelId} and not deleted`;
  if (!l) return "-1";
  await ctx.sql`insert into geobr_gdps.rate_votes (level_id, account_id, stars) values (${levelId}, ${account.account_id}, ${stars})
    on conflict (level_id, account_id) do update set stars = excluded.stars, at = now()`;
  return "1";
}

/** Applies a rating (shared by GD's mod buttons and the GeoBR admin panel). */
export async function applyRating(ctx: Ctx, actor: number, levelId: number, r: { stars: number; feature?: boolean; epic?: number; demonDiff?: number; coins?: boolean }) {
  const [l] = await ctx.sql`select account_id::int, stars, feature_score, epic, demon_diff, coins_verified from geobr_gdps.levels where level_id = ${levelId} and not deleted`;
  if (!l) return false;
  const stars = safeInt(r.stars, 0, 0, 10);
  const d = difficultyOfStars(stars);
  const feature = r.feature === undefined ? l.feature_score : (r.feature ? Math.max(1, l.feature_score) : 0);
  const epic = r.epic === undefined ? l.epic : safeInt(r.epic, 0, 0, 3);
  const demonDiff = d.demon ? (r.demonDiff === undefined ? l.demon_diff : DEMON_KEY43[safeInt(r.demonDiff, 3, 1, 5)]) : 0;
  const coins = r.coins === undefined ? l.coins_verified : r.coins;
  await ctx.sql`
    update geobr_gdps.levels set stars = ${stars}, difficulty = ${d.difficulty}, demon = ${d.demon}, demon_diff = ${demonDiff},
      feature_score = ${stars > 0 || epic > 0 ? feature : 0}, epic = ${stars > 0 ? epic : 0}, coins_verified = ${coins},
      rated_at = case when ${stars} > 0 then coalesce(rated_at, now()) else null end, rated_by = ${actor}, updated_at = now()
    where level_id = ${levelId}`;
  await recomputeCreatorPoints(ctx, l.account_id);
  await audit(ctx, actor, stars > 0 ? "level.rate" : "level.unrate", "level", levelId, { stars, feature: feature > 0, epic, demonDiff, coins, before: { stars: l.stars, feature: l.feature_score, epic: l.epic } });
  return true;
}

export async function rateGJDemon21(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const levelId = intParam(ctx, "levelID");
  const rating = safeInt(param(ctx, "rating"), 3, 1, 5);
  if (param(ctx, "mode") === "1") {
    if (!can(await grantsOf(ctx, account.account_id), "levels.rate")) return "-2";
    const res = await ctx.sql`update geobr_gdps.levels set demon_diff = ${DEMON_KEY43[rating]}, updated_at = now() where level_id = ${levelId} and demon and not deleted`;
    if (!res.count) return "-1";
    await audit(ctx, account.account_id, "level.demon", "level", levelId, { rating });
  }
  return String(levelId);
}

export async function suggestGJStars20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const levelId = intParam(ctx, "levelID");
  const stars = safeInt(param(ctx, "stars"), 0, 1, 10);
  // GD 2.2's moderator popup: tapping the difficulty face cycles
  // 0 rated -> 1 Featured -> 2 Epic -> 3 Legendary -> 4 Mythic.
  const tier = safeInt(param(ctx, "feature"), 0, 0, 4);
  const feature = tier > 0;
  const g = await grantsOf(ctx, account.account_id);
  if (can(g, "levels.rate")) {
    const tiers = can(g, "levels.feature");
    const ok = await applyRating(ctx, account.account_id, levelId, { stars, feature: tiers ? feature : undefined, epic: tiers ? Math.max(0, tier - 1) : undefined });
    return ok ? "1" : "-1";
  }
  if (!can(g, "levels.suggest")) return "-2";
  const [l] = await ctx.sql`select 1 from geobr_gdps.levels where level_id = ${levelId} and not deleted`;
  if (!l) return "-1";
  await ctx.sql`insert into geobr_gdps.suggestions (level_id, account_id, stars, feature) values (${levelId}, ${account.account_id}, ${stars}, ${feature})`;
  await audit(ctx, account.account_id, "level.suggest", "level", levelId, { stars, feature });
  return "1";
}

export async function getGJDailyLevel(ctx: Ctx): Promise<string> {
  const kind = safeInt(param(ctx, "type", param(ctx, "weekly", "0")), 0, 0, 2);
  const timely = await currentTimely(ctx, kind);
  if (!timely) return "-1";
  const id = timely.timely_id + TIMELY_OFFSET[kind];
  if (kind < 2) return `${id}|${secondsToReset(kind, ctx.now)}`;
  // Event: rewards in the chest format (XOR + base64, sha1 check); GD asks again soon.
  const chk = decodeChk(param(ctx, "chk"), XOR_CHESTS);
  const encoded = b64urlEncode(xorCipher(`SaKuJ:${chk}:${timely.timely_id}:3:${timely.rewards || "7,100"}`, XOR_CHESTS));
  return `${id}|10|SaKuJ${encoded}|${await sha1(encoded + SALT_CHESTS)}`;
}

async function scoreRows(ctx: Ctx, levelId: number, type: number, viewer: number | null, plat: false | "time" | "points") {
  const sql = ctx.sql;
  const conds = [sql`s.level_id = ${levelId}`, sql`not a.lb_banned`, sql`not a.is_banned`];
  if (type === 0) {
    if (!viewer) return [];
    conds.push(sql`(s.account_id = ${viewer} or s.account_id in (select b from geobr_gdps.friendships where a = ${viewer} union select a from geobr_gdps.friendships where b = ${viewer}))`);
  } else if (type === 2) conds.push(sql`s.updated_at > now() - interval '7 days'`);
  if (plat === "time") conds.push(sql`s.plat_time > 0`);
  else if (plat === "points") conds.push(sql`s.plat_points > 0`);
  else conds.push(sql`s.percent > 0`);
  const order = plat === "time" ? sql`s.plat_time asc` : plat === "points" ? sql`s.plat_points desc` : sql`s.percent desc, s.coins desc`;
  return await sql`
    select s.*, s.account_id::int as id, a.username, a.icon, a.color1, a.color2, a.icon_type, a.special
    from geobr_gdps.level_scores s join geobr_gdps.accounts a on a.account_id = s.account_id
    where ${conds.reduce((acc, c) => sql`${acc} and ${c}`)} order by ${order}, s.updated_at limit 100`;
}

function scoreList(ctx: Ctx, rows: Row[], value: (r: Row) => number): string {
  return rows.map((r, i) => record([[1, r.username], [2, r.id], [9, r.icon], [10, r.color1], [11, r.color2], [14, r.icon_type], [15, r.special], [16, r.id], [3, value(r)], [6, i + 1], [13, r.coins], [42, age(r.updated_at, ctx.now)]])).join("|");
}

export async function getGJLevelScores211(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  const levelId = intParam(ctx, "levelID");
  const percent = safeInt(param(ctx, "percent"), 0, 0, 100);
  if (account && percent > 0) {
    const attempts = safeInt(intParam(ctx, "s1") - 8354, 0, 0);
    const clicks = safeInt(intParam(ctx, "s2") - 3991, 0, 0);
    const seconds = safeInt(intParam(ctx, "s3") - 4085, 0, 0);
    const coins = safeInt(intParam(ctx, "s9") - 5819, 0, 0, 3);
    const [l] = await ctx.sql`select 1 from geobr_gdps.levels where level_id = ${levelId} and not deleted`;
    if (l) await ctx.sql`
      insert into geobr_gdps.level_scores (level_id, account_id, percent, attempts, clicks, seconds, coins)
      values (${levelId}, ${account.account_id}, ${percent}, ${attempts}, ${clicks}, ${seconds}, ${coins})
      on conflict (level_id, account_id) do update set
        percent = greatest(level_scores.percent, excluded.percent), coins = greatest(level_scores.coins, excluded.coins),
        attempts = excluded.attempts, clicks = excluded.clicks, seconds = excluded.seconds,
        updated_at = case when excluded.percent > level_scores.percent or excluded.coins > level_scores.coins then now() else level_scores.updated_at end`;
  }
  const rows = await scoreRows(ctx, levelId, safeInt(param(ctx, "type"), 1, 0, 2), account?.account_id ?? null, false);
  return scoreList(ctx, rows, (r) => r.percent);
}

export async function getGJLevelScoresPlat(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  const levelId = intParam(ctx, "levelID");
  const time = safeInt(param(ctx, "time"), 0, 0);
  const points = safeInt(param(ctx, "points"), 0, 0);
  if (account && (time > 0 || points > 0)) {
    const [l] = await ctx.sql`select 1 from geobr_gdps.levels where level_id = ${levelId} and not deleted`;
    if (l) await ctx.sql`
      insert into geobr_gdps.level_scores (level_id, account_id, percent, plat_time, plat_points)
      values (${levelId}, ${account.account_id}, 100, ${time}, ${points})
      on conflict (level_id, account_id) do update set
        plat_time = case when excluded.plat_time > 0 and (level_scores.plat_time = 0 or excluded.plat_time < level_scores.plat_time) then excluded.plat_time else level_scores.plat_time end,
        plat_points = greatest(level_scores.plat_points, excluded.plat_points), updated_at = now()`;
  }
  const mode = param(ctx, "mode") === "1" ? "points" : "time";
  const rows = await scoreRows(ctx, levelId, safeInt(param(ctx, "type"), 1, 0, 2), account?.account_id ?? null, mode);
  return scoreList(ctx, rows, (r) => mode === "points" ? r.plat_points : r.plat_time);
}
