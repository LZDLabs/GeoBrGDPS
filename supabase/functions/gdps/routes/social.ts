// Comments, profile posts, likes, friends, blocks and private messages.
import { type Ctx, audit, badges, can, gdAccount, grantsOf, intParam, param, rgb } from "../lib/core.ts";
import { age, b64urlDecode, clean, idList, record, safeInt } from "../lib/gd.ts";
import { muteReply, runCommand } from "./commands.ts";

// deno-lint-ignore no-explicit-any
type Row = Record<string, any>;

const B64 = /^[A-Za-z0-9+/=_-]*$/;

function textOk(b64: string, maxChars: number): boolean {
  if (!b64 || b64.length > maxChars * 4 + 8 || !B64.test(b64)) return false;
  const text = b64urlDecode(b64);
  return text.trim().length > 0 && [...text].length <= maxChars;
}

async function blockedEitherWay(ctx: Ctx, a: number, b: number): Promise<boolean> {
  const [row] = await ctx.sql`select 1 from geobr_gdps.blocks where (owner = ${a} and target = ${b}) or (owner = ${b} and target = ${a})`;
  return !!row;
}

async function areFriends(ctx: Ctx, x: number, y: number): Promise<boolean> {
  const [a, b] = x < y ? [x, y] : [y, x];
  const [row] = await ctx.sql`select 1 from geobr_gdps.friendships where a = ${a} and b = ${b}`;
  return !!row;
}

// ------------------------------------------------------------ level comments
export async function uploadGJComment21(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const body = param(ctx, "comment");
  if (!textOk(body, 100)) return "-1";
  const levelId = intParam(ctx, "levelID");
  const command = await runCommand(ctx, account, b64urlDecode(body), levelId);
  if (command !== null) return command;
  const muted = await muteReply(ctx, account.account_id);
  if (muted) return muted;
  const [l] = await ctx.sql`select 1 from geobr_gdps.levels where level_id = ${levelId} and not deleted and not blocked`;
  if (!l) return "-1";
  const [recent] = await ctx.sql`select 1 from geobr_gdps.comments where account_id = ${account.account_id} and created_at > now() - interval '5 seconds'`;
  if (recent) return "-1";
  const [row] = await ctx.sql`
    insert into geobr_gdps.comments (level_id, account_id, body, percent)
    values (${levelId}, ${account.account_id}, ${body}, ${safeInt(param(ctx, "percent"), 0, 0, 100)}) returning comment_id::int`;
  return String(row.comment_id);
}

async function commentList(ctx: Ctx, rows: Row[], withLevel: boolean, offset: number, count: number): Promise<string> {
  if (!rows.length) return "-2";
  const marks = await badges(ctx, [...new Set(rows.map((r) => r.account_id as number))]);
  const items = rows.map((c) => {
    const mark = marks.get(c.account_id);
    const parts: Array<[number, unknown]> = [];
    if (withLevel) parts.push([1, c.level_id]);
    parts.push([2, c.body], [3, c.account_id], [4, c.likes], [5, 0], [7, c.likes < -3 ? 1 : 0], [9, age(c.created_at, ctx.now)], [6, c.comment_id], [10, c.percent]);
    if (mark && mark.badge > 0) parts.push([11, mark.badge], [12, rgb(mark.color)]);
    const user = record([[1, clean(c.username)], [9, c.icon], [10, c.color1], [11, c.color2], [14, c.icon_type], [15, c.special], [16, c.account_id]], "~");
    return `${record(parts, "~")}:${user}`;
  });
  return `${items.join("|")}#${rows[0].total}:${offset}:${count}`;
}

export async function getGJComments21(ctx: Ctx): Promise<string> {
  const levelId = intParam(ctx, "levelID");
  const page = safeInt(param(ctx, "page"), 0, 0, 10000);
  const count = safeInt(param(ctx, "count"), 10, 1, 40);
  const order = param(ctx, "mode") === "1" ? ctx.sql`c.likes desc, c.comment_id desc` : ctx.sql`c.comment_id desc`;
  const rows = await ctx.sql`
    select c.comment_id::int, c.level_id::int, c.account_id::int, c.body, c.percent, c.likes, c.created_at,
      a.username, a.icon, a.color1, a.color2, a.icon_type, a.special, count(*) over ()::int as total
    from geobr_gdps.comments c join geobr_gdps.accounts a on a.account_id = c.account_id
    where c.level_id = ${levelId} and not c.deleted and not a.is_banned
    order by ${order} limit ${count} offset ${page * count}`;
  return commentList(ctx, rows, false, page * count, count);
}

export async function getGJCommentHistory(ctx: Ctx): Promise<string> {
  const target = intParam(ctx, "userID");
  const page = safeInt(param(ctx, "page"), 0, 0, 10000);
  const count = safeInt(param(ctx, "count"), 10, 1, 40);
  const [t] = await ctx.sql`select comment_state from geobr_gdps.accounts where account_id = ${target}`;
  if (!t) return "-1";
  if (t.comment_state > 0) {
    const viewer = await gdAccount(ctx);
    const self = viewer?.account_id === target;
    if (!self && (t.comment_state === 2 || !viewer || !(await areFriends(ctx, viewer.account_id, target)))) return "-2";
  }
  const order = param(ctx, "mode") === "1" ? ctx.sql`c.likes desc, c.comment_id desc` : ctx.sql`c.comment_id desc`;
  const rows = await ctx.sql`
    select c.comment_id::int, c.level_id::int, c.account_id::int, c.body, c.percent, c.likes, c.created_at,
      a.username, a.icon, a.color1, a.color2, a.icon_type, a.special, count(*) over ()::int as total
    from geobr_gdps.comments c join geobr_gdps.accounts a on a.account_id = c.account_id
    join geobr_gdps.levels l on l.level_id = c.level_id
    where c.account_id = ${target} and not c.deleted and not l.deleted and not l.blocked
    order by ${order} limit ${count} offset ${page * count}`;
  return commentList(ctx, rows, true, page * count, count);
}

export async function deleteGJComment20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const commentId = intParam(ctx, "commentID");
  const [c] = await ctx.sql`
    select c.account_id::int as author, l.account_id::int as level_owner from geobr_gdps.comments c
    join geobr_gdps.levels l on l.level_id = c.level_id where c.comment_id = ${commentId} and not c.deleted`;
  if (!c) return "-1";
  if (c.author !== account.account_id && c.level_owner !== account.account_id) {
    if (!can(await grantsOf(ctx, account.account_id), "comments.moderate")) return "-1";
    await audit(ctx, account.account_id, "comment.delete", "comment", commentId, { via: "gd" });
  }
  await ctx.sql`update geobr_gdps.comments set deleted = true where comment_id = ${commentId}`;
  return "1";
}

// ---------------------------------------------------------- profile posts
export async function uploadGJAccComment20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const body = param(ctx, "comment");
  if (!textOk(body, 140)) return "-1";
  const command = await runCommand(ctx, account, b64urlDecode(body), null);
  if (command !== null) return command;
  const muted = await muteReply(ctx, account.account_id);
  if (muted) return muted;
  const [recent] = await ctx.sql`select 1 from geobr_gdps.account_comments where account_id = ${account.account_id} and created_at > now() - interval '5 seconds'`;
  if (recent) return "-1";
  const [row] = await ctx.sql`insert into geobr_gdps.account_comments (account_id, body) values (${account.account_id}, ${body}) returning comment_id::int`;
  return String(row.comment_id);
}

export async function getGJAccountComments20(ctx: Ctx): Promise<string> {
  const target = intParam(ctx, "accountID");
  const page = safeInt(param(ctx, "page"), 0, 0, 10000);
  const rows = await ctx.sql`
    select comment_id::int, body, likes, created_at, count(*) over ()::int as total
    from geobr_gdps.account_comments where account_id = ${target} and not deleted
    order by comment_id desc limit 10 offset ${page * 10}`;
  if (!rows.length) return `#0:${page * 10}:10`;
  return `${rows.map((c) => record([[2, c.body], [4, c.likes], [9, age(c.created_at, ctx.now)], [6, c.comment_id]], "~")).join("|")}#${rows[0].total}:${page * 10}:10`;
}

export async function deleteGJAccComment20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const commentId = intParam(ctx, "commentID");
  const [c] = await ctx.sql`select account_id::int from geobr_gdps.account_comments where comment_id = ${commentId} and not deleted`;
  if (!c) return "-1";
  if (c.account_id !== account.account_id) {
    if (!can(await grantsOf(ctx, account.account_id), "comments.moderate")) return "-1";
    await audit(ctx, account.account_id, "account_comment.delete", "account_comment", commentId, { via: "gd" });
  }
  await ctx.sql`update geobr_gdps.account_comments set deleted = true where comment_id = ${commentId}`;
  return "1";
}

// ------------------------------------------------------------------ likes
export async function likeGJItem211(ctx: Ctx): Promise<string> {
  const type = safeInt(param(ctx, "type"), 1, 1, 4);
  const itemId = intParam(ctx, "itemID");
  const value = param(ctx, "like") === "0" ? -1 : 1;
  const account = await gdAccount(ctx);
  const udid = clean(param(ctx, "udid"), 64);
  const voter = account ? `a:${account.account_id}` : udid ? `u:${udid}` : "";
  if (!voter || itemId <= 0) return "-1";
  const inserted = await ctx.sql`
    insert into geobr_gdps.likes (item_type, item_id, voter, value) values (${type}, ${itemId}, ${voter}, ${value})
    on conflict do nothing returning 1`;
  if (!inserted.length) return "1";
  if (type === 1) await ctx.sql`update geobr_gdps.levels set likes = likes + ${value} where level_id = ${itemId}`;
  else if (type === 2) await ctx.sql`update geobr_gdps.comments set likes = likes + ${value} where comment_id = ${itemId}`;
  else if (type === 4) await ctx.sql`update geobr_gdps.level_lists set likes = likes + ${value} where list_id = ${itemId}`;
  else await ctx.sql`update geobr_gdps.account_comments set likes = likes + ${value} where comment_id = ${itemId}`;
  return "1";
}

// ---------------------------------------------------------------- friends
export async function uploadFriendRequest20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const target = intParam(ctx, "toAccountID");
  if (target === account.account_id) return "-1";
  const [t] = await ctx.sql`select friend_state from geobr_gdps.accounts where account_id = ${target} and not is_banned`;
  if (!t || t.friend_state !== 0) return "-1";
  if (await blockedEitherWay(ctx, account.account_id, target) || await areFriends(ctx, account.account_id, target)) return "-1";
  const body = param(ctx, "comment");
  const safe = body && textOk(body, 140) ? body : "";
  await ctx.sql`insert into geobr_gdps.friend_requests (from_account, to_account, body) values (${account.account_id}, ${target}, ${safe}) on conflict do nothing`;
  return "1";
}

export async function getGJFriendRequests20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const sent = param(ctx, "getSent") === "1";
  const page = safeInt(param(ctx, "page"), 0, 0, 10000);
  const rows = await ctx.sql`
    select r.request_id::int, r.body, r.is_new, r.created_at, a.account_id::int as id, a.username, a.icon, a.color1, a.color2, a.icon_type, a.special,
      count(*) over ()::int as total
    from geobr_gdps.friend_requests r join geobr_gdps.accounts a on a.account_id = ${sent ? ctx.sql`r.to_account` : ctx.sql`r.from_account`}
    where ${sent ? ctx.sql`r.from_account` : ctx.sql`r.to_account`} = ${account.account_id}
    order by r.request_id desc limit 10 offset ${page * 10}`;
  if (!rows.length) return "-2";
  return `${rows.map((r) => record([[1, r.username], [2, r.id], [9, r.icon], [10, r.color1], [11, r.color2], [14, r.icon_type], [15, r.special], [16, r.id], [32, r.request_id], [35, r.body], [41, r.is_new ? 1 : 0], [37, age(r.created_at, ctx.now)]])).join("|")}#${rows[0].total}:${page * 10}:10`;
}

export async function acceptGJFriendRequest20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const [r] = await ctx.sql`select from_account::int from geobr_gdps.friend_requests where request_id = ${intParam(ctx, "requestID")} and to_account = ${account.account_id}`;
  if (!r) return "-1";
  const [a, b] = r.from_account < account.account_id ? [r.from_account, account.account_id] : [account.account_id, r.from_account];
  await ctx.sql.begin(async (tx) => {
    await tx`insert into geobr_gdps.friendships (a, b) values (${a}, ${b}) on conflict do nothing`;
    await tx`delete from geobr_gdps.friend_requests where (from_account = ${a} and to_account = ${b}) or (from_account = ${b} and to_account = ${a})`;
  });
  return "1";
}

export async function deleteGJFriendRequests20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const sender = param(ctx, "isSender") === "1";
  const targets = ctx.params.has("accounts") ? idList(param(ctx, "accounts"), 100) : [intParam(ctx, "targetAccountID")];
  if (!targets.length) return "-1";
  const list = targets.join(",");
  if (sender) await ctx.sql`delete from geobr_gdps.friend_requests where from_account = ${account.account_id} and to_account = any(string_to_array(${list}, ',')::bigint[])`;
  else await ctx.sql`delete from geobr_gdps.friend_requests where to_account = ${account.account_id} and from_account = any(string_to_array(${list}, ',')::bigint[])`;
  return "1";
}

export async function readGJFriendRequest20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  await ctx.sql`update geobr_gdps.friend_requests set is_new = false where request_id = ${intParam(ctx, "requestID")} and to_account = ${account.account_id}`;
  return "1";
}

export async function removeGJFriend20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const target = intParam(ctx, "targetAccountID");
  const [a, b] = target < account.account_id ? [target, account.account_id] : [account.account_id, target];
  await ctx.sql`delete from geobr_gdps.friendships where a = ${a} and b = ${b}`;
  return "1";
}

export async function getGJUserList20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const me = account.account_id;
  let rows: Row[];
  if (param(ctx, "type") === "1") {
    rows = await ctx.sql`
      select a.account_id::int as id, a.username, a.icon, a.color1, a.color2, a.icon_type, a.special, false as is_new
      from geobr_gdps.blocks k join geobr_gdps.accounts a on a.account_id = k.target where k.owner = ${me} order by a.username`;
  } else {
    rows = await ctx.sql`
      select a.account_id::int as id, a.username, a.icon, a.color1, a.color2, a.icon_type, a.special,
        case when f.a = ${me} then f.new_for_a else f.new_for_b end as is_new
      from geobr_gdps.friendships f join geobr_gdps.accounts a on a.account_id = case when f.a = ${me} then f.b else f.a end
      where f.a = ${me} or f.b = ${me} order by a.username`;
    await ctx.sql`update geobr_gdps.friendships set new_for_a = case when a = ${me} then false else new_for_a end,
      new_for_b = case when b = ${me} then false else new_for_b end where a = ${me} or b = ${me}`;
  }
  if (!rows.length) return "-2";
  return rows.map((u) => record([[1, u.username], [2, u.id], [9, u.icon], [10, u.color1], [11, u.color2], [14, u.icon_type], [15, u.special], [16, u.id], [18, 0], [41, u.is_new ? 1 : 0]])).join("|");
}

export async function blockGJUser20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const target = intParam(ctx, "targetAccountID");
  if (target === account.account_id) return "-1";
  const [t] = await ctx.sql`select 1 from geobr_gdps.accounts where account_id = ${target}`;
  if (!t) return "-1";
  const [a, b] = target < account.account_id ? [target, account.account_id] : [account.account_id, target];
  await ctx.sql.begin(async (tx) => {
    await tx`insert into geobr_gdps.blocks (owner, target) values (${account.account_id}, ${target}) on conflict do nothing`;
    await tx`delete from geobr_gdps.friendships where a = ${a} and b = ${b}`;
    await tx`delete from geobr_gdps.friend_requests where (from_account = ${a} and to_account = ${b}) or (from_account = ${b} and to_account = ${a})`;
  });
  return "1";
}

export async function unblockGJUser20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  await ctx.sql`delete from geobr_gdps.blocks where owner = ${account.account_id} and target = ${intParam(ctx, "targetAccountID")}`;
  return "1";
}

// --------------------------------------------------------------- messages
export async function uploadGJMessage20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const target = intParam(ctx, "toAccountID");
  const subject = param(ctx, "subject");
  const body = param(ctx, "body");
  if (target === account.account_id || !textOk(subject, 35) || !body || body.length > 1200 || !B64.test(body)) return "-1";
  const [t] = await ctx.sql`select msg_state from geobr_gdps.accounts where account_id = ${target} and not is_banned`;
  if (!t || t.msg_state === 2) return "-1";
  if (await blockedEitherWay(ctx, account.account_id, target)) return "-1";
  if (t.msg_state === 1 && !(await areFriends(ctx, account.account_id, target))) return "-1";
  const [recent] = await ctx.sql`select 1 from geobr_gdps.messages where from_account = ${account.account_id} and created_at > now() - interval '3 seconds'`;
  if (recent) return "-1";
  await ctx.sql`insert into geobr_gdps.messages (from_account, to_account, subject, body) values (${account.account_id}, ${target}, ${subject}, ${body})`;
  return "1";
}

export async function getGJMessages20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const sent = param(ctx, "getSent") === "1";
  const page = safeInt(param(ctx, "page"), 0, 0, 10000);
  const me = account.account_id;
  const rows = sent
    ? await ctx.sql`
      select m.message_id::int, m.subject, m.is_read, m.created_at, a.account_id::int as id, a.username, count(*) over ()::int as total
      from geobr_gdps.messages m join geobr_gdps.accounts a on a.account_id = m.to_account
      where m.from_account = ${me} and not m.deleted_by_sender order by m.message_id desc limit 10 offset ${page * 10}`
    : await ctx.sql`
      select m.message_id::int, m.subject, m.is_read, m.created_at, a.account_id::int as id, a.username, count(*) over ()::int as total
      from geobr_gdps.messages m join geobr_gdps.accounts a on a.account_id = m.from_account
      where m.to_account = ${me} and not m.deleted_by_receiver order by m.message_id desc limit 10 offset ${page * 10}`;
  if (!rows.length) return "-2";
  return `${rows.map((m) => record([[6, m.username], [3, m.id], [2, m.id], [1, m.message_id], [4, m.subject], [8, m.is_read ? 1 : 0], [9, sent ? 1 : 0], [7, age(m.created_at, ctx.now)]])).join("|")}#${rows[0].total}:${page * 10}:10`;
}

export async function downloadGJMessage20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const me = account.account_id;
  const [m] = await ctx.sql`
    select m.*, m.message_id::int as mid, m.from_account::int as sender, m.to_account::int as receiver from geobr_gdps.messages m
    where m.message_id = ${intParam(ctx, "messageID")} and ((m.from_account = ${me} and not m.deleted_by_sender) or (m.to_account = ${me} and not m.deleted_by_receiver))`;
  if (!m) return "-1";
  const isSender = m.sender === me;
  const other = isSender ? m.receiver : m.sender;
  const [u] = await ctx.sql`select username from geobr_gdps.accounts where account_id = ${other}`;
  if (!isSender && !m.is_read) await ctx.sql`update geobr_gdps.messages set is_read = true where message_id = ${m.mid}`;
  return record([[6, u?.username ?? ""], [3, other], [2, other], [1, m.mid], [4, m.subject], [8, 1], [9, isSender ? 1 : 0], [5, m.body], [7, age(m.created_at, ctx.now)]]);
}

export async function deleteGJMessages20(ctx: Ctx): Promise<string> {
  const account = await gdAccount(ctx);
  if (!account) return "-1";
  const ids = ctx.params.has("messages") ? idList(param(ctx, "messages"), 100) : [intParam(ctx, "messageID")];
  if (!ids.length) return "-1";
  const list = ids.join(",");
  const me = account.account_id;
  await ctx.sql`update geobr_gdps.messages set deleted_by_sender = true where from_account = ${me} and message_id = any(string_to_array(${list}, ',')::bigint[])`;
  await ctx.sql`update geobr_gdps.messages set deleted_by_receiver = true where to_account = ${me} and message_id = any(string_to_array(${list}, ',')::bigint[])`;
  return "1";
}
