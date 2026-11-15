// Router: GD's "/database/<endpoint>.php" requests (form bodies, plain-text
// answers) and GeoBR's "/geobr/<endpoint>" JSON requests.
import type { Ctx, Env, Sql } from "./lib/core.ts";
import type { Store } from "./lib/storage.ts";
import * as accounts from "./routes/accounts.ts";
import * as levels from "./routes/levels.ts";
import * as social from "./routes/social.ts";
import * as songs from "./routes/songs.ts";
import * as geobr from "./routes/geobr.ts";
import { admin } from "./routes/admin.ts";
import * as packs from "./routes/packs.ts";
import * as rewards from "./routes/rewards.ts";

export const SERVER_VERSION = "1.11.0";
const MAX_BODY = 20 * 1024 * 1024;

type GdHandler = (ctx: Ctx) => Promise<string> | string;
type JsonHandler = (ctx: Ctx) => Promise<Record<string, unknown>>;

const unsupported: GdHandler = () => "-1";

export const GD_ROUTES: Record<string, GdHandler> = {
  registerGJAccount: accounts.registerGJAccount,
  loginGJAccount: accounts.loginGJAccount,
  backupGJAccountNew: accounts.backupGJAccountNew,
  syncGJAccountNew: accounts.syncGJAccountNew,
  getAccountURL: accounts.getAccountURL,
  updateGJAccSettings20: accounts.updateGJAccSettings20,
  getGJUserInfo20: accounts.getGJUserInfo20,
  getGJUsers20: accounts.getGJUsers20,
  updateGJUserScore22: accounts.updateGJUserScore22,
  getGJScores20: accounts.getGJScores20,
  requestUserAccess: accounts.requestUserAccess,

  getGJLevels21: levels.getGJLevels21,
  downloadGJLevel22: levels.downloadGJLevel22,
  uploadGJLevel21: levels.uploadGJLevel21,
  deleteGJLevelUser20: levels.deleteGJLevelUser20,
  updateGJDesc20: levels.updateGJDesc20,
  reportGJLevel: levels.reportGJLevel,
  rateGJStars211: levels.rateGJStars211,
  rateGJDemon21: levels.rateGJDemon21,
  suggestGJStars20: levels.suggestGJStars20,
  getGJDailyLevel: levels.getGJDailyLevel,
  getGJLevelScores211: levels.getGJLevelScores211,
  getGJLevelScoresPlat: levels.getGJLevelScoresPlat,

  uploadGJComment21: social.uploadGJComment21,
  getGJComments21: social.getGJComments21,
  getGJCommentHistory: social.getGJCommentHistory,
  deleteGJComment20: social.deleteGJComment20,
  uploadGJAccComment20: social.uploadGJAccComment20,
  getGJAccountComments20: social.getGJAccountComments20,
  deleteGJAccComment20: social.deleteGJAccComment20,
  likeGJItem211: social.likeGJItem211,
  uploadFriendRequest20: social.uploadFriendRequest20,
  getGJFriendRequests20: social.getGJFriendRequests20,
  acceptGJFriendRequest20: social.acceptGJFriendRequest20,
  deleteGJFriendRequests20: social.deleteGJFriendRequests20,
  readGJFriendRequest20: social.readGJFriendRequest20,
  removeGJFriend20: social.removeGJFriend20,
  getGJUserList20: social.getGJUserList20,
  blockGJUser20: social.blockGJUser20,
  unblockGJUser20: social.unblockGJUser20,
  uploadGJMessage20: social.uploadGJMessage20,
  getGJMessages20: social.getGJMessages20,
  downloadGJMessage20: social.downloadGJMessage20,
  deleteGJMessages20: social.deleteGJMessages20,

  getGJSongInfo: songs.getGJSongInfo,

  getGJMapPacks21: packs.getGJMapPacks21,
  getGJGauntlets21: packs.getGJGauntlets21,
  getGJLevelLists: packs.getGJLevelLists,
  uploadGJLevelList: packs.uploadGJLevelList,
  deleteGJLevelList: packs.deleteGJLevelList,
  getGJRewards: rewards.getGJRewards,
  getGJChallenges: rewards.getGJChallenges,

  // The mod sends these to RobTop's server; answered only if one arrives here anyway.
  getGJSecretReward: unsupported,
  getGJTopArtists: unsupported,
  getCustomContentURL: () => "https://geometrydashfiles.b-cdn.net",
};

export const JSON_ROUTES: Record<string, JsonHandler> = {
  info: async () => ({ ok: true, server: "GeoBR GDPS", version: SERVER_VERSION, songs: [songs.SONG_MIN, songs.SONG_MAX] }),
  me: geobr.me,
  "recovery/new": geobr.recoveryNew,
  "recovery/reset": geobr.recoveryReset,
  "owner/claim": geobr.ownerClaim,
  password: geobr.passwordChange,
  session: geobr.session,
  demonlist: geobr.demonList,
  "songs/search": songs.songsSearch,
  "songs/mine": songs.songsMine,
  "songs/upload": songs.songsUpload,
  "songs/confirm": songs.songsConfirm,
  admin,
};

/** "/functions/v1/gdps/database/accounts/loginGJAccount.php" -> {kind:"gd", name:"loginGJAccount"} */
export function resolve(pathname: string): { kind: "gd" | "json" | "root"; name: string } {
  let p = pathname;
  const at = p.indexOf("/gdps");
  if (at >= 0) p = p.slice(at + 5);
  p = p.replace(/^\/+/, "");
  if (p === "") return { kind: "root", name: "" };
  if (p.startsWith("geobr/")) return { kind: "json", name: p.slice(6).replace(/\/+$/, "") };
  p = p.replace(/^database\//, "").replace(/^accounts\//, "").replace(/\.php$/, "");
  return { kind: "gd", name: p };
}

export interface Deps { sql: Sql; store: Store; env: Env }

const text = (body: string, status = 200) => new Response(body, { status, headers: { "Content-Type": "text/plain; charset=utf-8" } });
const json = (body: unknown, status = 200) => new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });

export function createHandler(deps: Deps) {
  return async (req: Request): Promise<Response> => {
    const url = new URL(req.url);
    const route = resolve(url.pathname);
    if (route.kind === "root") return text("GeoBR GDPS");
    const length = Number(req.headers.get("content-length") ?? "0");
    if (length > MAX_BODY) return route.kind === "gd" ? text("-1", 413) : json({ ok: false, error: "Pedido grande demais." }, 413);

    let raw = "";
    if (req.method === "POST" || req.method === "PUT") raw = await req.text();
    if (raw.length > MAX_BODY) return text("-1", 413);
    const ip = (req.headers.get("cf-connecting-ip") ?? req.headers.get("x-forwarded-for")?.split(",")[0] ?? "local").trim().slice(0, 64);
    const ctx: Ctx = { ...deps, params: new URLSearchParams(url.search), json: {}, ip, now: new Date() };

    if (route.kind === "json") {
      const handler = JSON_ROUTES[route.name];
      if (!handler) return json({ ok: false, error: "Rota desconhecida." }, 404);
      if (raw) {
        try {
          const parsed = JSON.parse(raw);
          if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) ctx.json = parsed;
        } catch {
          return json({ ok: false, error: "JSON invalido." }, 400);
        }
      }
      try {
        return json(await handler(ctx));
      } catch (e) {
        console.error(`geobr/${route.name}`, e);
        return json({ ok: false, error: "Erro no servidor. Tente de novo." }, 500);
      }
    }

    const handler = GD_ROUTES[route.name];
    if (!handler) return text("-1", 404);
    for (const [k, v] of new URLSearchParams(raw)) ctx.params.set(k, v);
    try {
      return text(await handler(ctx));
    } catch (e) {
      console.error(route.name, e);
      return text("-1");
    }
  };
}
