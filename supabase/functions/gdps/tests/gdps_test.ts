// GDPS server tests: real local Postgres (fresh database with the migration),
// memory Storage, requests built exactly the way GD 2.2 sends them.
// Run with tests/run_local.sh.
import postgres from "postgres";
import { createHandler, resolve } from "../app.ts";
import { MemoryStore } from "../lib/storage.ts";
import { b64urlDecode, b64urlEncode, genMulti, genSolo, genSolo2, gjp2Of, sha1, xorCipher } from "../lib/gd.ts";

/** URL-safe base64 -> binary string (no UTF-8 decoding: XOR output is raw bytes). */
const b64urlDecodeRaw = (t: string) => { let s = t.replaceAll("-", "+").replaceAll("_", "/"); while (s.length % 4) s += "="; return atob(s); };

const sql = postgres({
  host: Deno.env.get("GDPS_TEST_HOST") ?? "127.0.0.1", port: Number(Deno.env.get("PGPORT") ?? 54329),
  database: Deno.env.get("GDPS_TEST_DB") ?? "gdps_test", username: Deno.env.get("PGUSER") ?? "postgres",
  prepare: false, max: 4, onnotice: () => {},
});
const store = new MemoryStore();
const handler = createHandler({
  sql, store,
  env: { supabaseUrl: "https://x.supabase.co", serviceKey: "", functionUrl: "https://x.supabase.co/functions/v1/gdps", songsBucket: "geobr-gdps-songs", savesBucket: "geobr-gdps-saves" },
});

let ipCounter = 0;
async function gd(endpoint: string, params: Record<string, string | number | boolean>, opts: { ip?: string; prefix?: string } = {}): Promise<string> {
  const body = new URLSearchParams();
  for (const [k, v] of Object.entries(params)) body.set(k, String(v));
  const path = opts.prefix ?? "/gdps/database/";
  const res = await handler(new Request(`http://local${path}${endpoint}.php`, {
    method: "POST", body: body.toString(),
    headers: { "Content-Type": "application/x-www-form-urlencoded", "x-forwarded-for": opts.ip ?? `10.0.0.${++ipCounter % 250}` },
  }));
  return await res.text();
}
// deno-lint-ignore no-explicit-any
async function api(path: string, body: Record<string, unknown>): Promise<any> {
  const res = await handler(new Request(`http://local/functions/v1/gdps/geobr/${path}`, { method: "POST", body: JSON.stringify(body), headers: { "x-forwarded-for": "10.9.9.9" } }));
  return await res.json();
}
function rec(text: string, sep = ":"): Map<string, string> {
  const parts = text.split(sep);
  const m = new Map<string, string>();
  for (let i = 0; i + 1 < parts.length; i += 2) m.set(parts[i], parts[i + 1]);
  return m;
}
function assert(cond: unknown, msg: string): asserts cond {
  if (!cond) throw new Error(`FAILED: ${msg}`);
}
function eq<T>(a: T, b: T, msg: string) {
  if (a !== b) throw new Error(`FAILED: ${msg} (got ${JSON.stringify(a)}, expected ${JSON.stringify(b)})`);
}

interface User { name: string; password: string; id: number; gjp2: string }
const users: Record<string, User> = {};
const auth = (u: User) => ({ accountID: u.id, gjp2: u.gjp2, secret: "Wmfd2893gb7", gameVersion: 22, binaryVersion: 45 });
const jauth = (u: User) => ({ accountID: u.id, gjp2: u.gjp2 });
const levelString = "H4sIAAAAAAAAC6WQwQ3DIAxFF3IlbGNIlFNmyAAMkBWyfCH-UdNLe-gBP_wfNjL7plrQdmyyGqlojRu0Ccx_FlJgTQmA1yJtAbUAS7l1YmlTQTnaEtmxHE6QTlA_OIGvA30pPUJPVzodSyQOHS8RqSqiXh1xjjPeQz8eT1ZTCh0RXx2Y_dHOwPsBsF5k3psBAAA=";

Deno.test({ name: "GeoBR GDPS server", sanitizeOps: false, sanitizeResources: false }, async (t) => {
  await t.step("router accepts GD's paths", () => {
    eq(resolve("/functions/v1/gdps/database/accounts/loginGJAccount.php").name, "loginGJAccount", "accounts path");
    eq(resolve("/gdps/getGJLevels21.php").name, "getGJLevels21", "plain path");
    eq(resolve("/gdps/database/getGJLevels21.php").name, "getGJLevels21", "database path");
    eq(resolve("/gdps/geobr/songs/search").kind, "json", "json route");
  });

  await t.step("register without e-mail and log in", async () => {
    for (const name of ["Alice", "Bobby", "Carol", "Owner", "Dave"]) {
      eq(await gd("accounts/registerGJAccount", { userName: name, password: `${name}-pass1`, email: "", secret: "Wmfv3899gc9" }), "1", `register ${name}`);
      const gjp2 = await gjp2Of(`${name}-pass1`);
      const res = await gd("accounts/loginGJAccount", { userName: name, gjp2, udid: "S1", secret: "Wmfv3899gc9" });
      const [id, uid] = res.split(",").map(Number);
      assert(id >= 1 && id === uid, `login ${name} -> ${res}`);
      users[name] = { name, password: `${name}-pass1`, id, gjp2 };
    }
    eq(users.Alice.id, 1, "account IDs start at 1 (each server has its own GD save)");
    eq(await gd("accounts/registerGJAccount", { userName: "alice", password: "whatever1" }), "-2", "username unique (case-insensitive)");
    eq(await gd("accounts/registerGJAccount", { userName: "a b", password: "whatever1" }), "-4", "invalid username");
    eq(await gd("accounts/registerGJAccount", { userName: "Shorty", password: "123" }), "-8", "short password");
    eq(await gd("accounts/registerGJAccount", { userName: "ab", password: "whatever1" }), "-9", "short username");
    eq(await gd("accounts/loginGJAccount", { userName: "Alice", gjp2: await gjp2Of("wrong") }), "-1", "wrong password");
    const [row] = await sql`select gjp2_hash from geobr_gdps.accounts where username = 'Alice'`;
    assert(row.gjp2_hash.startsWith("$2"), "password stored as bcrypt, never plain");
    const ip = "10.77.77.77";
    for (let i = 0; i < 5; i++) await gd("accounts/registerGJAccount", { userName: `Spam${i}x`, password: "whatever1" }, { ip });
    eq(await gd("accounts/registerGJAccount", { userName: "Spam9x", password: "whatever1" }, { ip }), "-1", "5 registrations per IP per hour");
  });

  await t.step("login brute force is limited per account", async () => {
    for (let i = 0; i < 10; i++) await gd("accounts/loginGJAccount", { userName: "Dave", gjp2: await gjp2Of(`bad${i}`) });
    eq(await gd("accounts/loginGJAccount", { userName: "Dave", gjp2: users.Dave.gjp2 }), "-1", "locked after 10 failures");
  });

  await t.step("cloud save: backup and sync through the account URL", async () => {
    const url = await gd("getAccountURL", { accountID: users.Alice.id, type: 1, secret: "Wmfd2893gb7" });
    eq(url, "https://x.supabase.co/functions/v1/gdps", "account URL is this function");
    const save = "H4sIGAMEMANAGER;H4sILOCALLEVELS";
    eq(await gd("accounts/backupGJAccountNew", { ...auth(users.Alice), saveData: save, gameVersion: 22, binaryVersion: 45 }, { prefix: "/functions/v1/gdps/database/" }), "1", "backup");
    eq(await gd("accounts/syncGJAccountNew", auth(users.Alice), { prefix: "/functions/v1/gdps/database/" }), `${save};22;45;a;a`, "sync returns the same save");
    eq(await gd("accounts/syncGJAccountNew", { accountID: users.Alice.id, gjp2: users.Bobby.gjp2 }), "-1", "another password cannot read it");
    eq(await gd("accounts/syncGJAccountNew", auth(users.Bobby)), "-1", "no save yet");
  });

  const level: Record<string, number> = {};
  await t.step("upload, update and search levels", async () => {
    const base = { ...auth(users.Alice), userName: "Alice", levelID: 0, levelVersion: 1, levelLength: 2, audioTrack: 0, auto: 0, password: "1123456", original: 0, twoPlayer: 0, songID: 0, objects: 1500, coins: 2, requestedStars: 5, unlisted: 0, ldm: 0, wt: 10, wt2: 20, extraString: "29_29_29_40_29_29", levelString, ts: 0, songIDs: "", sfxIDs: "", levelDesc: b64urlEncode("Primeira fase GeoBR") };
    const id = Number(await gd("uploadGJLevel21", { ...base, levelName: "Rio Run" }));
    eq(id, 1, "level IDs start at 1");
    level.rio = id;
    eq(Number(await gd("uploadGJLevel21", { ...base, levelName: "Rio Run", levelID: id, levelVersion: 2 })), id, "re-upload keeps the ID");
    const bobsId = Number(await gd("uploadGJLevel21", { ...base, ...auth(users.Bobby), levelName: "Rio Run", levelID: id }));
    assert(bobsId !== id && bobsId > 0, "another account cannot overwrite it (gets its own level)");
    level.bob = bobsId;
    level.hidden = Number(await gd("uploadGJLevel21", { ...base, levelName: "Segredo", unlisted: 1 }));
    level.demon = Number(await gd("uploadGJLevel21", { ...base, ...auth(users.Carol), levelName: "Inferno BR", levelLength: 3, requestedStars: 10 }));
    level.demon2 = Number(await gd("uploadGJLevel21", { ...base, ...auth(users.Carol), levelName: "Purgatorio", levelLength: 4 }));
    eq(await gd("uploadGJLevel21", { ...base, gjp2: users.Bobby.gjp2, levelName: "Fake" }), "-1", "wrong gjp2 cannot upload");

    const list = await gd("getGJLevels21", { type: 0, str: "rio run", page: 0, secret: "Wmfd2893gb7" });
    const parts = list.split("#");
    eq(parts.length, 5, "levels#creators#songs#page#hash");
    const levels = parts[0].split("|").map((x) => rec(x));
    eq(levels.length, 2, "both 'Rio Run' levels found");
    assert(parts[1].includes(`${users.Alice.id}:Alice:${users.Alice.id}`), "creator string");
    eq(parts[3], "2:0:10", "total:offset:10");
    const hash = await genMulti(levels.map((l) => ({ id: l.get("1")!, stars: Number(l.get("18")), coinsVerified: l.get("38") === "1" })));
    eq(parts[4], hash, "list chk hash");
    eq(b64urlDecode(levels[0].get("3")!), "Primeira fase GeoBR", "description");
    const byId = await gd("getGJLevels21", { type: 0, str: String(level.hidden) });
    assert(byId.startsWith(`1:${level.hidden}:`), "unlisted level found by exact ID");
    const recent = await gd("getGJLevels21", { type: 4, page: 0 });
    assert(!recent.includes(`1:${level.hidden}:`), "unlisted level hidden from lists");
    const mine = await gd("getGJLevels21", { ...auth(users.Alice), type: 5, str: String(users.Alice.id) });
    assert(mine.includes(`1:${level.hidden}:`), "owner sees own unlisted level");
    eq((await gd("getGJLevels21", { type: 0, str: "nada-assim" })), "-1", "no results");
    const filtered = await gd("getGJLevels21", { type: 4, len: "3", diff: "-" });
    assert(filtered.includes(`1:${level.demon}:`) && !filtered.includes(`1:${level.rio}:`), "length filter");
  });

  await t.step("download a level with GD's hashes", async () => {
    const res = await gd("downloadGJLevel22", { levelID: level.rio, inc: 1, secret: "Wmfd2893gb7" });
    const [data, h1, h2] = res.split("#");
    const l = rec(data);
    eq(l.get("4"), levelString, "level data");
    eq(l.get("2"), "Rio Run", "name");
    eq(l.get("5"), "2", "version updated");
    eq(h1, await genSolo(levelString), "genSolo hash");
    eq(h2, await genSolo2([users.Alice.id, 0, 0, level.rio, 0, 0, "1123456", 0].join(",")), "genSolo2 hash");
    eq(xorCipher(b64urlDecode(l.get("27")!), "26364"), "1123456", "level password is XOR+base64");
    const again = rec((await gd("downloadGJLevel22", { levelID: level.rio })).split("#")[0]);
    eq(Number(again.get("10")), Number(l.get("10")) + 1, "downloads counted");
    eq(await gd("downloadGJLevel22", { levelID: 123 }), "-1", "unknown level");
  });

  await t.step("description, report, delete", async () => {
    eq(await gd("updateGJDesc20", { ...auth(users.Alice), levelID: level.rio, levelDesc: b64urlEncode("Nova descricao") }), "1", "owner edits description");
    eq(await gd("updateGJDesc20", { ...auth(users.Bobby), levelID: level.rio, levelDesc: b64urlEncode("hack") }), "-1", "others cannot");
    eq(await gd("reportGJLevel", { levelID: level.rio }), "1", "report");
    eq(await gd("deleteGJLevelUser20", { ...auth(users.Alice), levelID: level.bob }), "-1", "cannot delete someone else's level");
    eq(await gd("deleteGJLevelUser20", { ...auth(users.Bobby), levelID: level.bob, secret: "Wmfv2898gc9" }), "1", "owner deletes");
    eq(await gd("downloadGJLevel22", { levelID: level.bob }), "-1", "deleted level is gone");
  });

  await t.step("comments, profile posts, likes", async () => {
    const c1 = Number(await gd("uploadGJComment21", { ...auth(users.Bobby), userName: "Bobby", comment: b64urlEncode("Que fase boa!"), levelID: level.rio, percent: 87 }));
    assert(c1 > 0, "comment posted");
    eq(await gd("uploadGJComment21", { ...auth(users.Bobby), comment: b64urlEncode("spam"), levelID: level.rio }), "-1", "5 s between comments");
    eq(await gd("uploadGJComment21", { ...auth(users.Carol), comment: b64urlEncode("x".repeat(101)), levelID: level.rio }), "-1", "100 characters max");
    const list = await gd("getGJComments21", { levelID: level.rio, page: 0, mode: 0 });
    const [items, page] = list.split("#");
    const [comment, user] = items.split(":");
    const c = rec(comment, "~"), u = rec(user, "~");
    eq(b64urlDecode(c.get("2")!), "Que fase boa!", "comment text");
    eq(c.get("10"), "87", "percent");
    eq(u.get("1"), "Bobby", "author");
    eq(page, "1:0:10", "comment paging");
    const history = await gd("getGJCommentHistory", { userID: users.Bobby.id, page: 0 });
    assert(history.startsWith(`1~${level.rio}~2~`), "comment history has the level ID");
    eq(await gd("likeGJItem211", { ...auth(users.Carol), itemID: level.rio, type: 1, like: 1 }), "1", "like level");
    await gd("likeGJItem211", { ...auth(users.Carol), itemID: level.rio, type: 1, like: 1 });
    await gd("likeGJItem211", { udid: "device-1", itemID: level.rio, type: 1, like: 0 });
    const [lv] = await sql`select likes from geobr_gdps.levels where level_id = ${level.rio}`;
    eq(lv.likes, 0, "one like + one dislike, repeated like ignored");
    eq(await gd("deleteGJComment20", { ...auth(users.Carol), commentID: c1, levelID: level.rio }), "-1", "random player cannot delete");
    eq(await gd("deleteGJComment20", { ...auth(users.Alice), commentID: c1, levelID: level.rio }), "1", "level owner deletes comments");
    eq(await gd("getGJComments21", { levelID: level.rio, page: 0 }), "-2", "no comments left");

    const post = Number(await gd("uploadGJAccComment20", { ...auth(users.Alice), comment: b64urlEncode("Oi GeoBR") }));
    const posts = await gd("getGJAccountComments20", { accountID: users.Alice.id, page: 0 });
    assert(posts.includes(`6~${post}`) && posts.endsWith("#1:0:10"), "profile post listed");
    eq(await gd("deleteGJAccComment20", { ...auth(users.Bobby), commentID: post }), "-1", "only the author deletes");
    eq(await gd("deleteGJAccComment20", { ...auth(users.Alice), commentID: post }), "1", "author deletes");
  });

  await t.step("friends, blocks and messages", async () => {
    eq(await gd("uploadFriendRequest20", { ...auth(users.Alice), toAccountID: users.Bobby.id, comment: b64urlEncode("bora jogar") }), "1", "friend request");
    const reqs = await gd("getGJFriendRequests20", { ...auth(users.Bobby), page: 0 });
    const r = rec(reqs.split("#")[0]);
    eq(r.get("16"), String(users.Alice.id), "request from Alice");
    const profile = rec(await gd("getGJUserInfo20", { ...auth(users.Bobby), targetAccountID: users.Alice.id }));
    eq(profile.get("31"), "3", "profile shows incoming request");
    eq(await gd("acceptGJFriendRequest20", { ...auth(users.Bobby), targetAccountID: users.Alice.id, requestID: r.get("32")! }), "1", "accept");
    const friends = await gd("getGJUserList20", { ...auth(users.Alice), type: 0 });
    assert(friends.includes(`1:Bobby:`) && friends.includes(":41:1"), "friend listed as new");
    assert((await gd("getGJUserList20", { ...auth(users.Alice), type: 0 })).includes(":41:0"), "new flag cleared after viewing");
    eq(rec(await gd("getGJUserInfo20", { ...auth(users.Alice), targetAccountID: users.Bobby.id })).get("31"), "1", "friend state");

    const subject = b64urlEncode("Oi");
    const body = b64urlEncode(xorCipher("mensagem secreta", "14251"));
    eq(await gd("uploadGJMessage20", { ...auth(users.Alice), toAccountID: users.Bobby.id, subject, body }), "1", "send message");
    const inbox = rec((await gd("getGJMessages20", { ...auth(users.Bobby), page: 0 })).split("#")[0]);
    eq(inbox.get("6"), "Alice", "inbox sender");
    eq(inbox.get("8"), "0", "unread");
    const own = rec(await gd("getGJUserInfo20", { ...auth(users.Bobby), targetAccountID: users.Bobby.id }));
    eq(own.get("38"), "1", "own profile counts new messages");
    const msg = rec(await gd("downloadGJMessage20", { ...auth(users.Bobby), messageID: inbox.get("1")! }));
    eq(xorCipher(b64urlDecode(msg.get("5")!), "14251"), "mensagem secreta", "message body round trip");
    eq(await gd("downloadGJMessage20", { ...auth(users.Carol), messageID: inbox.get("1")! }), "-1", "others cannot read it");
    eq(await gd("deleteGJMessages20", { ...auth(users.Bobby), messageID: inbox.get("1")! }), "1", "delete");
    eq(await gd("getGJMessages20", { ...auth(users.Bobby), page: 0 }), "-2", "inbox empty");

    eq(await gd("blockGJUser20", { ...auth(users.Bobby), targetAccountID: users.Alice.id }), "1", "block");
    eq(await gd("getGJUserList20", { ...auth(users.Alice), type: 0 }), "-2", "block removes the friendship");
    eq(await gd("uploadGJMessage20", { ...auth(users.Alice), toAccountID: users.Bobby.id, subject, body }), "-1", "blocked: no messages");
    eq(await gd("uploadFriendRequest20", { ...auth(users.Alice), toAccountID: users.Bobby.id }), "-1", "blocked: no requests");
    eq(await gd("getGJUserInfo20", { ...auth(users.Alice), targetAccountID: users.Bobby.id }), "-1", "blocked: profile hidden");
    eq(await gd("unblockGJUser20", { ...auth(users.Bobby), targetAccountID: users.Alice.id }), "1", "unblock");
    eq(await gd("updateGJAccSettings20", { ...auth(users.Carol), mS: 2, frS: 1, cS: 2, yt: "geobr", twitter: "", twitch: "" }), "1", "settings");
    eq(await gd("uploadGJMessage20", { ...auth(users.Alice), toAccountID: users.Carol.id, subject, body }), "-1", "messages closed by setting");
    eq(await gd("uploadFriendRequest20", { ...auth(users.Alice), toAccountID: users.Carol.id }), "-1", "requests closed by setting");
    eq(rec(await gd("getGJUserInfo20", { targetAccountID: users.Carol.id })).get("20"), "geobr", "youtube on profile");
  });

  await t.step("stats, leaderboards, user search", async () => {
    const stats = (u: User, stars: number, extra: Record<string, number> = {}) => gd("updateGJUserScore22", { ...auth(u), stars, moons: 3, demons: 1, diamonds: 50, icon: 4, iconType: 0, coins: 10, userCoins: 5, accIcon: 12, accShip: 3, accBall: 2, accBird: 1, accDart: 5, accRobot: 1, accGlow: 1, accSpider: 2, accExplosion: 1, accSwing: 1, accJetpack: 1, color1: 3, color2: 5, color3: 12, special: 2, sinfo: "1,2,3,4,5,6,0,0,0,0,0,0", sinfod: 2, sinfog: 0, dinfo: "", ...extra });
    eq(await stats(users.Alice, 500), String(users.Alice.id), "score update returns the user ID");
    await stats(users.Bobby, 900);
    await stats(users.Carol, 120);
    const top = (await gd("getGJScores20", { type: "top", count: 100 })).split("|").map((x) => rec(x));
    eq(top[0].get("1"), "Bobby", "top player");
    eq(top[0].get("6"), "1", "rank");
    eq(top[1].get("1"), "Alice", "second");
    const profile = rec(await gd("getGJUserInfo20", { targetAccountID: users.Alice.id }));
    eq(profile.get("30"), "2", "global rank on profile");
    eq(profile.get("21"), "12", "account icon");
    eq(profile.get("28"), "1", "glow");
    eq(profile.get("56"), "1,2,3,4,5,6,2,0", "levels info");
    await stats(users.Carol, 9000);
    const [flag] = await sql`select flagged from geobr_gdps.stat_history where account_id = ${users.Carol.id} order by id desc limit 1`;
    eq(flag.flagged, true, "big jump flagged for the leaderboard moderators");
    const search = rec((await gd("getGJUsers20", { str: "ali", page: 0 })).split("#")[0]);
    eq(search.get("1"), "Alice", "user search");
  });

  await t.step("Owner claim with the ADM password", async () => {
    let r = await api("owner/claim", { ...jauth(users.Owner), adminPassword: "errada" });
    eq(r.ok, false, "wrong ADM password");
    r = await api("owner/claim", { ...jauth(users.Owner), adminPassword: "adm-local-test" });
    eq(r.ok, true, "owner claimed");
    r = await api("owner/claim", { ...jauth(users.Alice), adminPassword: "adm-local-test" });
    eq(r.ok, false, "only one owner can claim");
    const me = await api("me", jauth(users.Owner));
    eq(me.owner, true, "owner flag");
    assert(me.permissions.includes("roles.manage") && me.permissions.includes("songs.moderate"), "owner has every permission");
    eq((await api("me", { accountID: users.Owner.id, gjp2: users.Alice.gjp2 })).ok, false, "me needs the right password");
  });

  const roleId: Record<string, number> = {};
  await t.step("roles: custom roles, hierarchy, server-side checks", async () => {
    const roles = await api("admin", { ...jauth(users.Owner), action: "roles.list" });
    for (const r of roles.roles) roleId[r.name] = r.role_id;
    eq(roles.roles.length, 7, "seven initial roles");
    assert(["Owner", "Admin", "Moderator", "Level Moderator", "Rate Moderator", "Event Manager", "Leaderboard Moderator"].every((n) => roleId[n]), "initial role names");
    eq((await api("admin", { ...jauth(users.Alice), action: "roles.list" })).ok, false, "players have no admin panel");
    eq((await api("admin", { ...jauth(users.Owner), action: "roles.grant", target: users.Bobby.id, role_id: roleId["Rate Moderator"] })).ok, true, "grant Rate Moderator");
    eq((await api("admin", { ...jauth(users.Owner), action: "roles.grant", target: users.Carol.id, role_id: roleId["Leaderboard Moderator"] })).ok, true, "grant Leaderboard Moderator");
    eq((await api("admin", { ...jauth(users.Owner), action: "roles.grant", target: users.Dave.id, role_id: roleId["Admin"] })).ok, true, "grant Admin");
    eq(await gd("requestUserAccess", auth(users.Bobby)), "1", "GD mod access for rate moderators");
    eq(await gd("requestUserAccess", auth(users.Alice)), "-1", "no access for players");
    eq((await api("admin", { ...jauth(users.Bobby), action: "accounts.ban", target: users.Alice.id })).error, "Sem permissao para essa acao.", "rate moderator cannot ban");
    eq((await api("admin", { ...jauth(users.Dave), action: "accounts.ban", target: users.Owner.id })).ok, false, "admin cannot ban the owner");
    eq((await api("admin", { ...jauth(users.Dave), action: "roles.grant", target: users.Alice.id, role_id: roleId["Moderator"] })).ok, false, "admin has no roles.manage by default");
    const custom = await api("admin", { ...jauth(users.Owner), action: "roles.save", name: "Curador", color: 0x00ff88, position: 35, permissions: ["admin.panel", "levels.suggest", "invalid.perm"] });
    eq(custom.ok, true, "owner creates a custom role");
    const after = await api("admin", { ...jauth(users.Owner), action: "roles.list" });
    const curador = after.roles.find((r: { name: string }) => r.name === "Curador");
    eq(JSON.stringify(curador.permissions), JSON.stringify(["admin.panel", "levels.suggest"]), "unknown permissions dropped");
    eq(curador.color, 0x00ff88, "custom colour");
    eq((await api("admin", { ...jauth(users.Owner), action: "roles.save", role_id: curador.role_id, name: "Curadoria", color: 255, position: 36, permissions: ["admin.panel"] })).ok, true, "edit role");
    eq((await api("admin", { ...jauth(users.Owner), action: "roles.delete", role_id: roleId["Owner"] })).ok, false, "owner role cannot be deleted");
    eq((await api("admin", { ...jauth(users.Owner), action: "roles.revoke", target: users.Owner.id, role_id: roleId["Owner"] })).ok, false, "last owner stays");
    eq((await api("admin", { ...jauth(users.Owner), action: "roles.delete", role_id: curador.role_id })).ok, true, "delete custom role");
    const badge = rec(await gd("getGJUserInfo20", { targetAccountID: users.Bobby.id }));
    eq(badge.get("49"), "1", "moderator badge on the profile");
  });

  await t.step("rating: GD mod button, Epic/Legendary/Mythic, creator points", async () => {
    eq(await gd("suggestGJStars20", { ...auth(users.Alice), levelID: level.rio, stars: 5, feature: 1 }), "-2", "players cannot rate");
    eq(await gd("suggestGJStars20", { ...auth(users.Bobby), levelID: level.rio, stars: 5, feature: 1 }), "1", "rate moderator rates from GD");
    let l = rec((await gd("downloadGJLevel22", { levelID: level.rio })).split("#")[0]);
    eq(l.get("18"), "5", "stars");
    eq(l.get("9"), "30", "hard difficulty");
    assert(Number(l.get("19")) > 0, "featured");
    eq(l.get("42"), "0", "feature 1 = Featured only");
    // GD's moderator popup: tapping the face cycles Featured(1) Epic(2) Legendary(3) Mythic(4).
    for (const [tier, epic] of [[2, "1"], [3, "2"], [4, "3"], [0, "0"]] as const) {
      eq(await gd("suggestGJStars20", { ...auth(users.Bobby), levelID: level.rio, stars: 5, feature: tier }), "1", `rate with feature ${tier}`);
      l = rec((await gd("downloadGJLevel22", { levelID: level.rio })).split("#")[0]);
      eq(l.get("42"), epic, `feature ${tier} -> key 42 = ${epic}`);
      eq(Number(l.get("19")) > 0, tier > 0, `feature ${tier} -> featured flag`);
    }
    eq((await api("admin", { ...jauth(users.Bobby), action: "levels.feature", levelID: level.rio, epic: 3 })).ok, true, "mythic");
    l = rec((await gd("downloadGJLevel22", { levelID: level.rio })).split("#")[0]);
    eq(l.get("42"), "3", "mythic tier in key 42");
    const mythic = await gd("getGJLevels21", { type: 4, mythic: 1 });
    assert(mythic.includes(`1:${level.rio}:`), "mythic filter");
    const [cp] = await sql`select creator_points from geobr_gdps.accounts where account_id = ${users.Alice.id}`;
    eq(cp.creator_points, 5, "rated+featured+epic+legendary+mythic = 5 CP");
    eq((await api("admin", { ...jauth(users.Bobby), action: "levels.rate", levelID: level.demon, stars: 10, demonDiff: 5 })).ok, true, "rate demon");
    eq((await api("admin", { ...jauth(users.Bobby), action: "levels.rate", levelID: level.demon2, stars: 10, demonDiff: 2 })).ok, true, "rate second demon");
    l = rec((await gd("downloadGJLevel22", { levelID: level.demon })).split("#")[0]);
    eq(l.get("17"), "1", "demon flag");
    eq(l.get("43"), "6", "extreme demon key 43");
    const extreme = await gd("getGJLevels21", { type: 4, diff: "-2", demonFilter: 5 });
    assert(extreme.includes(`1:${level.demon}:`) && !extreme.includes(`1:${level.demon2}:`), "demon difficulty filter");
    eq(await gd("rateGJDemon21", { ...auth(users.Bobby), levelID: level.demon2, rating: 4, mode: 1 }), String(level.demon2), "GD demon rate (mod)");
    const awarded = await gd("getGJLevels21", { type: 11 });
    assert(awarded.includes(`1:${level.rio}:`) && awarded.includes(`1:${level.demon}:`), "awarded list");
    eq(await gd("rateGJStars211", { ...auth(users.Alice), levelID: level.demon, stars: 9 }), "1", "player star vote");
  });

  await t.step("moderation: block levels, delete comments, bans", async () => {
    eq((await api("admin", { ...jauth(users.Bobby), action: "levels.block", levelID: level.demon2 })).ok, false, "rate moderator cannot block");
    eq((await api("admin", { ...jauth(users.Dave), action: "levels.block", levelID: level.demon2, reason: "copia" })).ok, true, "admin blocks");
    eq(await gd("downloadGJLevel22", { levelID: level.demon2 }), "-1", "blocked level hidden");
    eq((await api("admin", { ...jauth(users.Dave), action: "levels.unblock", levelID: level.demon2 })).ok, true, "unblock");
    const cid = Number(await gd("uploadGJComment21", { ...auth(users.Alice), comment: b64urlEncode("comentario ruim"), levelID: level.demon }));
    const recent = await api("admin", { ...jauth(users.Dave), action: "comments.recent", levelID: level.demon });
    eq(recent.comments[0].id, cid, "recent comments");
    eq((await api("admin", { ...jauth(users.Dave), action: "comments.delete", commentID: cid })).ok, true, "moderator deletes comment");
    eq((await api("admin", { ...jauth(users.Dave), action: "accounts.ban", target: users.Alice.id, reason: "teste", days: 0 })).ok, true, "ban");
    eq(await gd("accounts/loginGJAccount", { userName: "Alice", gjp2: users.Alice.gjp2 }), "-12", "banned login");
    eq(await gd("uploadGJComment21", { ...auth(users.Alice), comment: b64urlEncode("oi"), levelID: level.demon }), "-1", "banned cannot post");
    eq((await api("admin", { ...jauth(users.Dave), action: "accounts.unban", target: users.Alice.id })).ok, true, "unban");
    await sql`update geobr_gdps.accounts set is_banned = true, banned_until = now() - interval '1 minute' where account_id = ${users.Alice.id}`;
    eq(await gd("accounts/loginGJAccount", { userName: "Alice", gjp2: users.Alice.gjp2 }), `${users.Alice.id},${users.Alice.id}`, "temporary ban expires");
  });

  await t.step("leaderboard moderator: investigate, remove scores, leaderboard-only ban", async () => {
    await gd("getGJLevelScores211", { ...auth(users.Alice), levelID: level.demon, percent: 64, s1: 8354 + 30, s2: 3991 + 400, s3: 4085 + 90, s9: 5819 + 1, type: 1 });
    await gd("getGJLevelScores211", { ...auth(users.Bobby), levelID: level.demon, percent: 100, s1: 8354 + 3, s9: 5819 + 3, type: 1 });
    let scores = (await gd("getGJLevelScores211", { ...auth(users.Carol), levelID: level.demon, type: 1 })).split("|").map((x) => rec(x));
    eq(scores[0].get("1"), "Bobby", "best score first");
    eq(scores[0].get("3"), "100", "percent");
    eq(scores[1].get("13"), "1", "coins");
    const hist = await api("admin", { ...jauth(users.Carol), action: "lb.history", target: users.Bobby.id });
    assert(hist.ok && hist.history.length >= 1, "stats history");
    const flagged = await api("admin", { ...jauth(users.Carol), action: "lb.flagged" });
    assert(flagged.flagged.some((f: { id: number }) => f.id === users.Carol.id), "flagged list");
    eq((await api("admin", { ...jauth(users.Carol), action: "scores.remove", levelID: level.demon, target: users.Bobby.id })).ok, true, "remove fraudulent score");
    scores = (await gd("getGJLevelScores211", { levelID: level.demon, type: 1 })).split("|").map((x) => rec(x));
    eq(scores[0].get("1"), "Alice", "score removed");
    eq((await api("admin", { ...jauth(users.Carol), action: "lb.ban", target: users.Alice.id, reason: "hack" })).ok, true, "leaderboard ban");
    eq(await gd("getGJLevelScores211", { levelID: level.demon, type: 1 }), "", "banned player out of level leaderboards");
    const top = await gd("getGJScores20", { type: "top" });
    assert(!top.includes("1:Alice:"), "out of the global leaderboard");
    eq(await gd("accounts/loginGJAccount", { userName: "Alice", gjp2: users.Alice.gjp2 }), `${users.Alice.id},${users.Alice.id}`, "but can still play and log in");
    eq((await api("admin", { ...jauth(users.Carol), action: "accounts.ban", target: users.Alice.id })).ok, false, "leaderboard moderator cannot ban accounts");
    eq((await api("admin", { ...jauth(users.Carol), action: "lb.ban", target: users.Owner.id })).ok, false, "cannot touch the owner");
    eq((await api("admin", { ...jauth(users.Carol), action: "lb.unban", target: users.Alice.id })).ok, true, "lift leaderboard ban");
  });

  await t.step("Daily, Weekly and Event", async () => {
    eq(await gd("getGJDailyLevel", { type: 0 }), "-1", "no daily yet");
    eq((await api("admin", { ...jauth(users.Bobby), action: "events.set", kind: 0, levelID: level.rio })).ok, false, "rate moderator cannot set events");
    const d = await api("admin", { ...jauth(users.Dave), action: "events.set", kind: 0, levelID: level.rio });
    const w = await api("admin", { ...jauth(users.Dave), action: "events.set", kind: 1, levelID: level.demon });
    eq((await api("admin", { ...jauth(users.Dave), action: "events.set", kind: 2, levelID: level.demon2 })).ok, false, "event needs rewards");
    const e = await api("admin", { ...jauth(users.Dave), action: "events.set", kind: 2, levelID: level.demon2, rewards: "500 diamantes 1000 orbs" });
    assert(d.ok && w.ok && e.ok && !d.queued && !w.queued, "daily, weekly and event set");
    eq(e.rewards, "8,500,7,1000", "event rewards in GD item codes");
    // Daily is a queue: the next level waits for tomorrow, today's stays.
    const next = await api("admin", { ...jauth(users.Dave), action: "events.set", kind: 0, levelID: level.demon });
    assert(next.ok && next.queued && new Date(next.starts_at).getTime() > Date.now(), "second daily is queued for tomorrow");
    eq((await api("admin", { ...jauth(users.Dave), action: "events.set", kind: 0, levelID: level.demon })).ok, false, "a level is queued once");
    const [dailyId, left] = (await gd("getGJDailyLevel", { type: 0 })).split("|").map(Number);
    eq(dailyId, d.id, "daily id");
    assert(left > 0 && left <= 86400, "time left until the next daily (UTC midnight)");
    eq(Number((await gd("getGJDailyLevel", { type: 1 })).split("|")[0]), w.id + 100001, "weekly offset");
    const ev = (await gd("getGJDailyLevel", { type: 2, chk: "abcde" + btoa(xorCipher("777", "59182")) })).split("|");
    eq(Number(ev[0]), e.id + 200001, "event offset");
    eq(ev[1], "10", "event asks again soon");
    eq(ev[3], await sha1(ev[2].slice(5) + "pC26fpYaQCtg"), "event reward hash");
    const plain = xorCipher(b64urlDecodeRaw(ev[2].slice(5)), "59182").split(":");
    eq(plain[1], "777", "event chk echoed");
    eq(plain.slice(4).join(":"), "8,500,7,1000", "event rewards");
    // A new event replaces the current one.
    const e2 = await api("admin", { ...jauth(users.Dave), action: "events.set", kind: 2, levelID: level.rio, rewards: "8 50" });
    eq(Number((await gd("getGJDailyLevel", { type: 2 })).split("|")[0]), e2.id + 200001, "latest event replaces the old one");
    const daily = (await gd("downloadGJLevel22", { levelID: -1 })).split("#");
    const l = rec(daily[0]);
    eq(l.get("1"), String(level.rio), "daily download is the level");
    eq(l.get("41"), String(d.id), "key 41");
    eq(daily[2], await genSolo2([users.Alice.id, 5, 0, level.rio, 0, l.get("19"), "1123456", d.id].join(",")), "daily hash");
    eq(daily[3], `${users.Alice.id}:Alice:${users.Alice.id}`, "daily creator suffix");
    eq(rec((await gd("downloadGJLevel22", { levelID: -2 })).split("#")[0]).get("41"), String(w.id + 100001), "weekly key 41");
    assert((await gd("getGJLevels21", { type: 22 })).includes(`1:${level.demon}:`), "past weeklies list");
  });

  await t.step("Demon List", async () => {
    eq((await api("admin", { ...jauth(users.Dave), action: "demonlist.set", levelID: level.rio })).ok, false, "only demons");
    await api("admin", { ...jauth(users.Dave), action: "demonlist.set", levelID: level.demon2, verifier: "Carol" });
    await api("admin", { ...jauth(users.Dave), action: "demonlist.set", levelID: level.demon, position: 1, verifier: "Carol" });
    let list = await api("demonlist", {});
    eq(list.levels.map((x: { level_id: number }) => x.level_id).join(","), `${level.demon},${level.demon2}`, "insert at #1 pushes the others down");
    await api("admin", { ...jauth(users.Dave), action: "demonlist.remove", levelID: level.demon });
    list = await api("demonlist", {});
    eq(list.levels[0].position, 1, "positions close the gap");
  });

  await t.step("Music Library: upload, moderation, GD song info, permanent IDs", async () => {
    const mp3 = new Uint8Array(4096); mp3.set([0x49, 0x44, 0x33, 3]); mp3[100] = 7;
    const up = await api("songs/upload", { ...jauth(users.Alice), name: "Samba do Cubo", artist: "GeoBR Band" });
    assert(up.ok && up.songID >= 8000001 && up.songID <= 9999999, `song id in range: ${up.songID}`);
    assert(up.uploadUrl.includes("/upload/geobr-gdps-songs/songs/"), "signed upload URL");
    const path = up.uploadUrl.split("/upload/geobr-gdps-songs/")[1].split("?")[0];
    await store.put("geobr-gdps-songs", path, mp3, "audio/mpeg");
    const ok = await api("songs/confirm", { ...jauth(users.Alice), songID: up.songID });
    eq(ok.status, "pending", "player uploads wait for approval");
    eq(await gd("getGJSongInfo", { songID: up.songID }), "-1", "pending song not served");
    eq((await api("songs/search", { q: "samba" })).total, 0, "pending song not in search");
    const dup = await api("songs/upload", { ...jauth(users.Bobby), name: "Copia", artist: "X" });
    await store.put("geobr-gdps-songs", dup.uploadUrl.split("/upload/geobr-gdps-songs/")[1].split("?")[0], mp3, "audio/mpeg");
    const dupRes = await api("songs/confirm", { ...jauth(users.Bobby), songID: dup.songID });
    eq(dupRes.duplicateOf, up.songID, "same MP3 is never stored twice");
    const bad = await api("songs/upload", { ...jauth(users.Bobby), name: "Nao e mp3", artist: "X" });
    await store.put("geobr-gdps-songs", bad.uploadUrl.split("/upload/geobr-gdps-songs/")[1].split("?")[0], new TextEncoder().encode("<html>".repeat(500)), "audio/mpeg");
    eq((await api("songs/confirm", { ...jauth(users.Bobby), songID: bad.songID })).ok, false, "non-MP3 rejected");
    eq((await api("admin", { ...jauth(users.Bobby), action: "songs.review", songID: up.songID, status: "approved" })).ok, false, "rate moderator cannot approve songs");
    eq((await api("admin", { ...jauth(users.Dave), action: "songs.review", songID: up.songID, status: "approved" })).ok, true, "admin approves");
    eq((await api("admin", { ...jauth(users.Dave), action: "songs.edit", songID: up.songID, name: "Samba do Cubo (GD)", artist: "GeoBR Band" })).ok, true, "edit");
    const info = await gd("getGJSongInfo", { songID: up.songID, secret: "Wmfd2893gb7" });
    const fields = info.split("~|~");
    const song = new Map<string, string>();
    for (let i = 0; i + 1 < fields.length; i += 2) song.set(fields[i], fields[i + 1]);
    eq(song.get("1"), String(up.songID), "song id");
    eq(song.get("2"), "Samba do Cubo (GD)", "song name");
    eq(song.get("4"), "GeoBR Band", "artist");
    eq(decodeURIComponent(song.get("10")!), `https://storage.test/public/geobr-gdps-songs/${path}`, "download URL");
    const search = await api("songs/search", { q: "geobr band" });
    eq(search.songs[0].id, up.songID, "search by artist");
    eq((await api("songs/search", { q: String(up.songID) })).songs[0].name, "Samba do Cubo (GD)", "search by ID");
    // A level using the song lists it in the "#songs#" section.
    const lid = Number(await gd("uploadGJLevel21", { ...auth(users.Alice), levelName: "Samba Run", levelString, songID: up.songID, levelLength: 1, password: "0" }));
    const list = (await gd("getGJLevels21", { type: 0, str: String(lid) })).split("#");
    assert(list[2].startsWith(`1~|~${up.songID}~|~2~|~Samba do Cubo (GD)`), "songs section");
    eq(await gd("getGJSongInfo", { songID: 1234567 }), "-1", "Newgrounds IDs are not answered here");
    eq((await api("admin", { ...jauth(users.Dave), action: "songs.review", songID: up.songID, status: "removed" })).ok, true, "remove");
    eq(await gd("getGJSongInfo", { songID: up.songID }), "-2", "removed song disabled");
    const next = await api("songs/upload", { ...jauth(users.Carol), name: "Outra", artist: "Y" });
    assert(next.songID > bad.songID, "IDs are never reused");
    eq((await api("songs/mine", jauth(users.Alice))).songs[0].status, "removed", "my uploads with status");
    const own = await api("songs/upload", { ...jauth(users.Owner), name: "Do Owner", artist: "Z" });
    const ownMp3 = mp3.slice(); ownMp3[200] = 9;
    await store.put("geobr-gdps-songs", own.uploadUrl.split("/upload/geobr-gdps-songs/")[1].split("?")[0], ownMp3, "audio/mpeg");
    eq((await api("songs/confirm", { ...jauth(users.Owner), songID: own.songID })).status, "approved", "moderators publish directly");
  });

  await t.step("recovery code resets the password", async () => {
    const r = await api("recovery/new", jauth(users.Bobby));
    assert(/^[A-Z2-9]{4}(-[A-Z2-9]{4}){3}$/.test(r.code), "16-character code");
    eq((await api("recovery/reset", { userName: "Bobby", code: "AAAA-AAAA-AAAA-AAAA", password: "novaSenha1" })).ok, false, "wrong code");
    eq((await api("recovery/reset", { userName: "Bobby", code: r.code.toLowerCase(), password: "123" })).ok, false, "weak password refused");
    eq((await api("recovery/reset", { userName: "bobby", code: r.code.toLowerCase(), password: "novaSenha1" })).ok, true, "reset");
    eq(await gd("accounts/loginGJAccount", { userName: "Bobby", gjp2: users.Bobby.gjp2 }), "-1", "old password stops working");
    const gjp2 = await gjp2Of("novaSenha1");
    eq(await gd("accounts/loginGJAccount", { userName: "Bobby", gjp2 }), `${users.Bobby.id},${users.Bobby.id}`, "new password works");
    eq((await api("recovery/reset", { userName: "Bobby", code: r.code, password: "outraSenha1" })).ok, false, "code is single-use");
    users.Bobby.gjp2 = gjp2;
  });

  await t.step("audit log and statistics", async () => {
    eq((await api("admin", { ...jauth(users.Bobby), action: "audit.list" })).ok, false, "rate moderator cannot read the audit log");
    const actions = new Set<string>();
    for (let page = 0; page < 20; page++) {
      const log = await api("admin", { ...jauth(users.Owner), action: "audit.list", page });
      if (!log.entries.length) break;
      log.entries.forEach((e: { action: string }) => actions.add(e.action));
    }
    for (const a of ["owner.claim", "role.grant", "level.rate", "level.block", "account.ban", "leaderboard.ban", "score.remove", "event.set", "song.approved", "song.removed", "comment.delete", "demonlist.set"]) assert(actions.has(a), `audit has ${a}`);
    const stats = await api("admin", { ...jauth(users.Dave), action: "stats" });
    assert(stats.stats.accounts >= 5 && stats.stats.rated >= 3 && stats.stats.songs >= 1, "server statistics");
  });

  await t.step("Map Packs and Gauntlets", async () => {
    eq(await gd("getGJGauntlets21", { special: 1 }), "-1", "no gauntlets yet");
    let packs = await gd("getGJMapPacks21", { page: 0 });
    assert(packs.startsWith("#0:0:10#"), "empty map pack page");
    eq((await api("admin", { ...jauth(users.Bobby), action: "packs.save", name: "X", levels: String(level.rio) })).ok, false, "rate moderator cannot make packs");
    const pack = await api("admin", { ...jauth(users.Dave), action: "packs.save", name: "Pack Brasil", levels: `${level.rio},${level.demon},999999`, stars: 6, coins: 2, difficulty: 3, text_color: "#ffcc00" });
    eq(pack.ok, true, "admin creates a map pack");
    packs = await gd("getGJMapPacks21", { page: 0, secret: "Wmfd2893gb7" });
    const [body, page, hash] = packs.split("#");
    const p = rec(body);
    eq(p.get("2"), "Pack Brasil", "pack name");
    eq(p.get("3"), `${level.rio},${level.demon}`, "only existing levels, in order");
    eq(p.get("7"), "255,204,0", "colour from hex");
    eq(page, "1:0:10", "pack paging");
    const id = String(pack.id);
    eq(hash, await sha1(id[0] + id[id.length - 1] + "6" + "2" + "xI25fpAapCQg"), "genPack hash");
    const extra: number[] = [];
    for (let i = 0; i < 3; i++) extra.push(Number(await gd("uploadGJLevel21", { ...auth(users.Dave), levelName: `Gauntlet ${i}`, levelString, levelLength: 1, password: "0" })));
    eq((await api("admin", { ...jauth(users.Dave), action: "gauntlets.save", gauntlet_id: 1, levels: `${level.rio},${level.demon}` })).ok, false, "gauntlet needs 5 levels");
    const five = [level.rio, level.demon, ...extra];
    eq((await api("admin", { ...jauth(users.Dave), action: "gauntlets.save", gauntlet_id: 1, levels: five.join(",") })).ok, true, "fire gauntlet");
    const g = await gd("getGJGauntlets21", { special: 1 });
    eq(g, `1:1:3:${five.join(",")}#${await sha1(`1${five.join(",")}xI25fpAapCQg`)}`, "gauntlet record and hash");
    const opened = await gd("getGJLevels21", { type: 10, str: five.join(",") });
    eq(opened.split("#")[0].split("|").length, 5, "GD opens the gauntlet levels by ID list");
  });

  await t.step("Level lists", async () => {
    const lid = Number(await gd("uploadGJLevelList", { ...auth(users.Alice), listName: "Favoritas", listDesc: b64urlEncode("as melhores"), listLevels: `${level.demon},${level.rio}`, difficulty: -1, listVersion: 0, original: 0, unlisted: 0, secret: "Wmfd2893gb7" }));
    assert(lid >= 1, `list uploaded ${lid}`);
    eq(Number(await gd("uploadGJLevelList", { ...auth(users.Alice), listID: lid, listName: "Favoritas 2", listLevels: `${level.demon},${level.rio}`, listVersion: 2 })), lid, "update keeps the ID");
    const lists = (await gd("getGJLevelLists", { type: 4, page: 0 })).split("#");
    const l = rec(lists[0]);
    eq(l.get("2"), "Favoritas 2", "list name");
    eq(l.get("51"), `${level.demon},${level.rio}`, "list levels");
    eq(l.get("50"), "Alice", "list creator");
    assert(lists[1].includes(`${users.Alice.id}:Alice:${users.Alice.id}`), "creators section");
    const inList = (await gd("getGJLevels21", { type: 25, str: lid })).split("#")[0].split("|").map((x) => rec(x).get("1"));
    eq(inList.join(","), `${level.demon},${level.rio}`, "type 25 returns the list levels in order");
    await gd("likeGJItem211", { ...auth(users.Bobby), itemID: lid, type: 4, like: 1 });
    eq((await api("admin", { ...jauth(users.Bobby), action: "lists.rate", listID: lid, stars: 5, featured: 1 })).ok, true, "rate a list");
    const rated = rec((await gd("getGJLevelLists", { type: 11 })).split("#")[0]);
    eq(rated.get("55"), "5", "list stars");
    eq(rated.get("14"), "1", "list like");
    eq(await gd("deleteGJLevelList", { ...auth(users.Carol), listID: lid }), "-1", "others cannot delete");
    eq(await gd("deleteGJLevelList", { ...auth(users.Alice), listID: lid }), "1", "owner deletes");
    eq(await gd("getGJLevelLists", { type: 0, str: String(lid) }), "-1", "deleted list gone");
  });

  await t.step("chests and quests from the GDPS", async () => {
    const chk = "abcde" + btoa(xorCipher("12345", "59182"));
    const decode = (res: string, key: string) => {
      const [enc, hash] = res.slice(5).split("|");
      const plain = xorCipher(atob(enc.replaceAll("-", "+").replaceAll("_", "/")), key);
      return { plain, enc, hash };
    };
    let r = decode(await gd("getGJRewards", { ...auth(users.Carol), udid: "dev1", chk, rewardType: 0 }), "59182");
    let f = r.plain.split(":");
    eq(f[2], "12345", "chk echoed back");
    eq(f[4], String(users.Carol.id), "account ID");
    eq(f[5], "0", "small chest ready");
    eq(r.hash, await sha1(r.enc + "pC26fpYaQCtg"), "chest hash");
    r = decode(await gd("getGJRewards", { ...auth(users.Carol), udid: "dev1", chk, rewardType: 1 }), "59182");
    f = r.plain.split(":");
    eq(f[5], String(4 * 3600), "small chest opened, 4 h cooldown");
    eq(f[7], "1", "one small chest opened");
    eq(f[6].split(",").length, 4, "orbs,diamonds,shard,keys");
    eq(await gd("getGJRewards", { ...auth(users.Carol), udid: "dev1", chk, rewardType: 1 }), "-1", "cannot open it again");
    const qchk = "fghij" + btoa(xorCipher("777", "19847"));
    const q = decode(await gd("getGJChallenges", { ...auth(users.Carol), udid: "dev1", chk: qchk }), "19847");
    const qf = q.plain.split(":");
    eq(qf[2], "777", "quest chk echoed");
    eq(qf.length, 9, "header + 3 quests");
    eq(qf[6].split(",").length, 5, "id,type,amount,reward,name");
    eq(q.hash, await sha1(q.enc + "oC36fpYaPtdg"), "quest hash");
  });

  await t.step("comment commands and mutes", async () => {
    const lvl = Number(await gd("uploadGJLevel21", { ...auth(users.Carol), levelName: "Comando", levelString, levelLength: 2, password: "0" }));
    await new Promise((r) => setTimeout(r, 5100));
    const res = await gd("uploadGJComment21", { ...auth(users.Bobby), comment: b64urlEncode("!rate 4"), levelID: lvl });
    assert(res.startsWith("temp_0_") && res.includes("4 estrela"), `!rate reply ${res}`);
    eq(rec((await gd("downloadGJLevel22", { levelID: lvl })).split("#")[0]).get("18"), "4", "level rated by command");
    eq(await gd("getGJComments21", { levelID: lvl }), "-2", "the command is not posted as a comment");
    assert((await gd("uploadGJComment21", { ...auth(users.Bobby), comment: b64urlEncode("!epic"), levelID: lvl })).startsWith("temp_0_"), "!epic");
    eq(rec((await gd("downloadGJLevel22", { levelID: lvl })).split("#")[0]).get("42"), "1", "epic by command");
    const daily = await gd("uploadGJComment21", { ...auth(users.Bobby), comment: b64urlEncode("!daily"), levelID: lvl });
    assert(daily.includes("Erro"), "rate moderator cannot set the daily");
    const slash = await gd("uploadGJComment21", { ...auth(users.Bobby), comment: b64urlEncode("/rate 4"), levelID: lvl });
    assert(slash.startsWith("temp_0_") && slash.includes("4 estrela"), `/rate works like !rate (${slash})`);
    const queued = await gd("uploadGJComment21", { ...auth(users.Dave), comment: b64urlEncode("/daily"), levelID: lvl });
    assert(queued.startsWith("temp_0_") && queued.includes("fila"), `/daily goes to the queue (${queued})`);
    const asPlayer = await gd("uploadGJComment21", { ...auth(users.Alice), comment: b64urlEncode("!rate 10"), levelID: lvl });
    assert(/^\d+$/.test(asPlayer), "a player's ! text is just a comment");
    eq(rec((await gd("downloadGJLevel22", { levelID: lvl })).split("#")[0]).get("18"), "4", "player cannot rate");
    const lb = await gd("uploadGJAccComment20", { ...auth(users.Carol), comment: b64urlEncode("!lbban alice trapaca") });
    assert(lb.startsWith("temp_0_"), `!lbban from a profile post ${lb}`);
    const [a] = await sql`select lb_banned from geobr_gdps.accounts where account_id = ${users.Alice.id}`;
    eq(a.lb_banned, true, "leaderboard ban by command");
    await gd("uploadGJAccComment20", { ...auth(users.Carol), comment: b64urlEncode("!lbunban Alice") });
    const mute = await gd("uploadGJComment21", { ...auth(users.Dave), comment: b64urlEncode("!mute Alice 2 spam"), levelID: lvl });
    assert(mute.startsWith("temp_0_"), "!mute");
    await new Promise((r) => setTimeout(r, 5100));
    const muted = await gd("uploadGJComment21", { ...auth(users.Alice), comment: b64urlEncode("oi"), levelID: lvl });
    assert(/^temp_\d+_spam$/.test(muted) && Number(muted.split("_")[1]) > 7000, `muted reply ${muted}`);
    eq((await api("admin", { ...jauth(users.Dave), action: "accounts.unmute", target: users.Alice.id })).ok, true, "unmute");
    assert(/^\d+$/.test(await gd("uploadGJComment21", { ...auth(users.Alice), comment: b64urlEncode("voltei"), levelID: lvl })), "can comment again");
    assert((await gd("uploadGJComment21", { ...auth(users.Bobby), comment: b64urlEncode("!help"), levelID: lvl })).includes("/daily"), "!help lists the / commands");
  });

  await t.step("password change keeps the account", async () => {
    eq((await api("password", { ...jauth(users.Carol), password: "12" })).ok, false, "weak password refused");
    const r = await api("password", { ...jauth(users.Carol), password: "CarolNova9" });
    eq(r.ok, true, "changed");
    eq(r.gjp2, await gjp2Of("CarolNova9"), "returns the new gjp2 for GD");
    eq(await gd("accounts/loginGJAccount", { userName: "Carol", gjp2: users.Carol.gjp2 }), "-1", "old password stops working");
    users.Carol.gjp2 = r.gjp2;
    eq(await gd("accounts/loginGJAccount", { userName: "Carol", gjp2: r.gjp2 }), `${users.Carol.id},${users.Carol.id}`, "new password works");
  });

  await t.step("account rename by the player", async () => {
    eq((await api("account/rename", { ...jauth(users.Carol), userName: "a b" })).ok, false, "invalid name");
    eq((await api("account/rename", { ...jauth(users.Carol), userName: "alice" })).ok, false, "name taken (any case)");
    const r = await api("account/rename", { ...jauth(users.Carol), userName: "CarolBR" });
    eq(r.ok, true, "renamed");
    eq(await gd("accounts/loginGJAccount", { userName: "CarolBR", gjp2: users.Carol.gjp2 }), `${users.Carol.id},${users.Carol.id}`, "logs in with the new name");
    eq(rec(await gd("getGJUserInfo20", { targetAccountID: users.Carol.id })).get("1"), "CarolBR", "profile shows the new name");
    eq((await api("account/rename", { ...jauth(users.Carol), userName: "CarolDois" })).ok, false, "once a day");
  });

  await t.step("Supabase profile link needs the server key (live test covers GoTrue)", async () => {
    eq((await api("session", { ...jauth(users.Alice), accessToken: "x" })).ok, false, "no key, no link");
  });

  await t.step("locked down: API roles cannot touch geobr_gdps", async () => {
    const [r] = await sql`select has_schema_privilege('anon', 'geobr_gdps', 'usage') as anon, has_schema_privilege('authenticated', 'geobr_gdps', 'usage') as auth,
      (select bool_and(rowsecurity) from pg_tables where schemaname = 'geobr_gdps') as rls`;
    eq(r.anon, false, "anon has no access");
    eq(r.auth, false, "authenticated has no access");
    eq(r.rls, true, "RLS on every table");
  });

  await sql.end();
});
