// Map Packs, Gauntlets and level lists (GD 2.2 "Lists").
import { type Ctx, audit, can, gdAccount, grantsOf, intParam, param } from "../lib/core.ts";
import { clean, genPack, idList, record, safeInt, sha1, SALT_LEVEL } from "../lib/gd.ts";

// deno-lint-ignore no-explicit-any
type Row = Record<string, any>;

// ------------------------------------------------------------ map packs
export async function getGJMapPacks21(ctx: Ctx): Promise<string> {
  const page = safeInt(param(ctx, "page"), 0, 0, 1000);
  const rows = await ctx.sql`
    select pack_id::int as id, name, level_ids, stars, coins, difficulty, text_color, bar_color, count(*) over ()::int as total
    from geobr_gdps.map_packs order by position, pack_id limit 10 offset ${page * 10}`;
  const packs = rows.map((p) => record([[1, p.id], [2, clean(p.name, 40)], [3, p.level_ids], [4, p.stars], [5, p.coins], [6, p.difficulty], [7, p.text_color], [8, p.bar_color]])).join("|");
  const hash = await genPack(rows.map((p) => ({ id: p.id, stars: p.stars, coins: p.coins })));
  return `${packs}#${rows[0]?.total ?? 0}:${page * 10}:10#${hash}`;
}

// ------------------------------------------------------------ gauntlets
export async function getGJGauntlets21(ctx: Ctx): Promise<string> {
  const rows = await ctx.sql`select gauntlet_id, level_ids from geobr_gdps.gauntlets order by gauntlet_id`;
  const full = rows.filter((g) => idList(g.level_ids, 5).length === 5);
  if (!full.length) return "-1";
  const text = full.map((g) => record([[1, g.gauntlet_id], [3, idList(g.level_ids, 5).join(",")]])).join("|");
  const chk = full.map((g) => `${g.gauntlet_id}${idList(g.level_ids, 5).join(",")}`).join("");
  return `${text}#${await sha1(chk + SALT_LEVEL)}`;
}

// ---------------------------------------------------------- level lists
function listRecord(l: Row): string {
  return record([
    [1, l.id], [2, clean(l.name, 40)], [3, l.description], [5, l.version], [49, l.owner], [50, clean(l.username)],
    [10, l.downloads], [7, l.difficulty], [14, l.likes], [19, l.featured], [51, l.level_ids], [55, l.stars],
    [56, l.count_for_reward], [28, Math.floor(new Date(l.created_at).getTime() / 1000)], [29, Math.floor(new Date(l.updated_at).getTime() / 1000)],
  ]);
}

export async function uploadGJLevelList(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const levels = idList(param(ctx, "listLevels"), 100);
  if (!levels.length) return "-6";
  const fields = {
    name: clean(param(ctx, "listName"), 40).trim() || "Lista sem nome",
    description: param(ctx, "listDesc").replace(/[^A-Za-z0-9+/=_-]/g, "").slice(0, 400),
    level_ids: levels.join(","),
    difficulty: safeInt(param(ctx, "difficulty"), -1, -1, 10),
    version: Math.max(1, safeInt(param(ctx, "listVersion"), 1, 0, 100000)),
    original: safeInt(param(ctx, "original"), 0, 0),
    unlisted: safeInt(param(ctx, "unlisted"), 0, 0, 2),
  };
  const listId = intParam(ctx, "listID", 0);
  if (listId > 0) {
    const res = await ctx.sql`update geobr_gdps.level_lists set ${ctx.sql(fields)}, updated_at = now()
      where list_id = ${listId} and account_id = ${account.account_id} and not deleted`;
    if (res.count) return String(listId);
  }
  const [recent] = await ctx.sql`select count(*)::int as n from geobr_gdps.level_lists where account_id = ${account.account_id} and created_at > now() - interval '1 hour'`;
  if ((recent?.n ?? 0) >= 10) return "-9";
  const [row] = await ctx.sql`insert into geobr_gdps.level_lists ${ctx.sql({ ...fields, account_id: account.account_id })} returning list_id::int`;
  return String(row.list_id);
}

export async function deleteGJLevelList(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const listId = intParam(ctx, "listID");
  const [l] = await ctx.sql`select account_id::int from geobr_gdps.level_lists where list_id = ${listId} and not deleted`;
  if (!l) return "-1";
  if (l.account_id !== account.account_id) {
    if (!can(await grantsOf(ctx, account.account_id), "levels.delete")) return "-1";
    await audit(ctx, account.account_id, "list.delete", "list", listId, { via: "gd" });
  }
  await ctx.sql`update geobr_gdps.level_lists set deleted = true where list_id = ${listId}`;
  return "1";
}

export async function getGJLevelLists(ctx: Ctx): Promise<string> {
  const sql = ctx.sql;
  const type = intParam(ctx, "type", 0);
  const str = param(ctx, "str").trim();
  const page = safeInt(param(ctx, "page"), 0, 0, 5000);
  const viewer = await gdAccount(ctx);
  const conds = [sql`not l.deleted`];
  let order = sql`l.list_id desc`;
  let publicOnly = true;
  switch (type) {
    case 0:
      if (/^\d+$/.test(str)) { conds.push(sql`l.list_id = ${Number(str)}`); publicOnly = false; }
      else if (str) conds.push(sql`lower(l.name) like ${"%" + str.toLowerCase().replace(/[%_\\]/g, "").slice(0, 40) + "%"}`);
      order = sql`l.likes desc, l.list_id desc`;
      break;
    case 1: order = sql`l.downloads desc, l.list_id desc`; break;
    case 2: order = sql`l.likes desc, l.list_id desc`; break;
    case 3: conds.push(sql`l.created_at > now() - interval '7 days'`); order = sql`l.likes desc, l.list_id desc`; break;
    case 4: break;
    case 5: {
      const owner = safeInt(str, 0);
      conds.push(sql`l.account_id = ${owner}`);
      if (viewer && viewer.account_id === owner) publicOnly = false;
      break;
    }
    case 6: case 7: case 11: conds.push(sql`(l.stars > 0 or l.featured > 0)`); order = sql`l.featured desc, l.updated_at desc`; break;
    case 12: {
      const ids = idList(param(ctx, "followed"), 500);
      if (!ids.length) return "-1";
      conds.push(sql`l.account_id = any(string_to_array(${ids.join(",")}, ',')::bigint[])`);
      break;
    }
    case 13:
      if (!viewer) return "-1";
      conds.push(sql`l.account_id in (select b from geobr_gdps.friendships where a = ${viewer.account_id} union select a from geobr_gdps.friendships where b = ${viewer.account_id})`);
      break;
    default: break;
  }
  if (publicOnly) conds.push(sql`l.unlisted = 0`);
  if (param(ctx, "star") === "1") conds.push(sql`l.stars > 0`);
  const diff = param(ctx, "diff", "-");
  if (diff && diff !== "-") {
    const ds = diff.split(",").map((x) => safeInt(x, -99)).filter((x) => x >= -1 && x <= 10);
    if (ds.length) conds.push(sql`l.difficulty = any(string_to_array(${ds.join(",")}, ',')::int[])`);
  }
  const rows = await sql`
    select l.*, l.list_id::int as id, l.account_id::int as owner, a.username, count(*) over ()::int as total
    from geobr_gdps.level_lists l join geobr_gdps.accounts a on a.account_id = l.account_id
    where ${conds.reduce((acc, c) => sql`${acc} and ${c}`)} order by ${order} limit 10 offset ${page * 10}`;
  if (!rows.length) return "-1";
  const creators = new Map<number, string>();
  for (const l of rows) creators.set(l.owner, `${l.owner}:${clean(l.username)}:${l.owner}`);
  return `${rows.map(listRecord).join("|")}#${[...creators.values()].join("|")}#${rows[0].total}:${page * 10}:10#${await sha1(rows.map((l) => l.id).join(",") + SALT_LEVEL)}`;
}

/** Levels of a list, in the list's order (getGJLevels21 type 25). */
export async function listLevelIds(ctx: Ctx, listId: number): Promise<number[] | null> {
  const [l] = await ctx.sql`update geobr_gdps.level_lists set downloads = downloads + 1 where list_id = ${listId} and not deleted returning level_ids`;
  return l ? idList(l.level_ids, 100) : null;
}
