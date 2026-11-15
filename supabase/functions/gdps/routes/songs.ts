// GeoBR Music Library. Songs have their own permanent IDs (8000001..9999999,
// from a sequence that never cycles), are stored as MP3 in the public bucket
// and are served to GD through getGJSongInfo, like Newgrounds songs.
import { type Ctx, audit, can, grantsOf, intParam, jsonAccount } from "../lib/core.ts";
import { clean, safeInt, sha256 } from "../lib/gd.ts";

export const SONG_MIN = 8000001;
export const SONG_MAX = 9999999;
const MAX_SONG_BYTES = 15 * 1024 * 1024;
const UPLOADS_PER_DAY = 5;

export const isGeoBRSong = (id: number) => id >= SONG_MIN && id <= SONG_MAX;

// deno-lint-ignore no-explicit-any
type Row = Record<string, any>;

function songRecord(ctx: Ctx, s: Row): string {
  const url = ctx.store.publicUrl(ctx.env.songsBucket, s.object_path);
  const size = (Number(s.size_bytes) / 1048576).toFixed(2);
  const fields: Array<[number, unknown]> = [[1, s.id], [2, clean(s.name, 60)], [3, s.uploader ?? 1], [4, clean(s.artist, 40)], [5, size], [6, ""], [10, encodeURIComponent(url)], [7, ""], [8, 1]];
  return fields.map(([k, v]) => `${k}~|~${v}`).join("~|~");
}

/** The "#songs#" part of a level list: GeoBR songs used by these levels. */
export async function songsSection(ctx: Ctx, ids: number[]): Promise<string> {
  const ours = [...new Set(ids.filter(isGeoBRSong))];
  if (!ours.length) return "";
  const rows = await ctx.sql`
    select song_id::int as id, name, artist, uploader::int, object_path, size_bytes from geobr_gdps.songs
    where song_id = any(string_to_array(${ours.join(",")}, ',')::bigint[]) and status = 'approved'`;
  return rows.map((s) => songRecord(ctx, s)).join("~:~");
}

export async function getGJSongInfo(ctx: Ctx): Promise<string> {
  const id = intParam(ctx, "songID");
  if (!isGeoBRSong(id)) return "-1";
  const [s] = await ctx.sql`select song_id::int as id, name, artist, uploader::int, object_path, size_bytes, status from geobr_gdps.songs where song_id = ${id}`;
  if (!s) return "-1";
  if (s.status === "blocked" || s.status === "removed") return "-2";
  if (s.status !== "approved") return "-1";
  return songRecord(ctx, s);
}

function publicSong(ctx: Ctx, s: Row) {
  return {
    id: s.id, name: s.name, artist: s.artist, uploader: s.uploader_name ?? null,
    size_mb: Number((Number(s.size_bytes) / 1048576).toFixed(2)), status: s.status,
    url: ctx.store.publicUrl(ctx.env.songsBucket, s.object_path),
  };
}

export async function songsSearch(ctx: Ctx) {
  const q = String(ctx.json.q ?? "").trim().slice(0, 60);
  const page = safeInt(ctx.json.page, 0, 0, 1000);
  const asId = /^\d+$/.test(q) ? Number(q) : -1;
  const like = "%" + q.toLowerCase().replace(/[%_\\]/g, "") + "%";
  const rows = await ctx.sql`
    select s.song_id::int as id, s.name, s.artist, s.object_path, s.size_bytes, s.status, a.username as uploader_name, count(*) over ()::int as total
    from geobr_gdps.songs s left join geobr_gdps.accounts a on a.account_id = s.uploader
    where s.status = 'approved' and (${q === ""} or s.song_id = ${asId} or lower(s.name) like ${like} or lower(s.artist) like ${like})
    order by (s.song_id = ${asId}) desc, s.song_id desc limit 20 offset ${page * 20}`;
  return { ok: true, total: rows[0]?.total ?? 0, songs: rows.map((s) => publicSong(ctx, s)) };
}

export async function songsMine(ctx: Ctx) {
  const account = await jsonAccount(ctx);
  if (!account) return { ok: false, error: "Faca login na conta GeoBR." };
  const rows = await ctx.sql`
    select song_id::int as id, name, artist, object_path, size_bytes, status from geobr_gdps.songs
    where uploader = ${account.account_id} and status <> 'uploading' order by song_id desc limit 50`;
  return { ok: true, songs: rows.map((s) => publicSong(ctx, s)) };
}

export async function songsUpload(ctx: Ctx) {
  const account = await jsonAccount(ctx);
  if (!account) return { ok: false, error: "Faca login na conta GeoBR." };
  const name = clean(ctx.json.name, 60).trim();
  const artist = clean(ctx.json.artist, 40).trim();
  if (!name || !artist) return { ok: false, error: "Informe nome e artista." };
  const [recent] = await ctx.sql`select count(*)::int as n from geobr_gdps.songs where uploader = ${account.account_id} and created_at > now() - interval '1 day'`;
  if ((recent?.n ?? 0) >= UPLOADS_PER_DAY) return { ok: false, error: `Limite de ${UPLOADS_PER_DAY} envios por dia.` };
  const path = `songs/${crypto.randomUUID()}.mp3`;
  const [row] = await ctx.sql`
    insert into geobr_gdps.songs (name, artist, uploader, object_path, status)
    values (${name}, ${artist}, ${account.account_id}, ${path}, 'uploading') returning song_id::int as id`;
  const uploadUrl = await ctx.store.signUpload(ctx.env.songsBucket, path);
  return { ok: true, songID: row.id, uploadUrl, maxBytes: MAX_SONG_BYTES };
}

function looksLikeMp3(b: Uint8Array): boolean {
  if (b.length < 4) return false;
  if (b[0] === 0x49 && b[1] === 0x44 && b[2] === 0x33) return true;     // "ID3"
  return b[0] === 0xff && (b[1] & 0xe0) === 0xe0;                         // MPEG frame sync
}

export async function songsConfirm(ctx: Ctx) {
  const account = await jsonAccount(ctx);
  if (!account) return { ok: false, error: "Faca login na conta GeoBR." };
  const id = safeInt(ctx.json.songID, 0);
  const [s] = await ctx.sql`select song_id::int as id, object_path, status from geobr_gdps.songs where song_id = ${id} and uploader = ${account.account_id}`;
  if (!s || s.status !== "uploading") return { ok: false, error: "Envio nao encontrado." };
  const bytes = await ctx.store.get(ctx.env.songsBucket, s.object_path);
  const fail = async (error: string) => {
    await ctx.store.remove(ctx.env.songsBucket, s.object_path);
    await ctx.sql`update geobr_gdps.songs set status = 'removed' where song_id = ${id}`;
    return { ok: false, error };
  };
  if (!bytes) return fail("O arquivo nao chegou ao servidor.");
  if (bytes.length > MAX_SONG_BYTES) return fail("Arquivo maior que 15 MB.");
  if (bytes.length < 1024 || !looksLikeMp3(bytes)) return fail("O arquivo nao e um MP3 valido.");
  const hash = await sha256(bytes);
  const [dup] = await ctx.sql`select song_id::int as id from geobr_gdps.songs where sha256 = ${hash} and status <> 'removed' and song_id <> ${id}`;
  if (dup) {
    const result = await fail(`Essa musica ja existe na biblioteca (ID ${dup.id}).`);
    return { ...result, duplicateOf: dup.id };
  }
  const moderator = can(await grantsOf(ctx, account.account_id), "songs.moderate");
  const status = moderator ? "approved" : "pending";
  await ctx.sql`
    update geobr_gdps.songs set size_bytes = ${bytes.length}, sha256 = ${hash}, status = ${status},
      published_at = case when ${moderator} then now() else null end, reviewed_by = case when ${moderator} then ${account.account_id}::bigint else null end
    where song_id = ${id}`;
  if (moderator) await audit(ctx, account.account_id, "song.approve", "song", id, { auto: true });
  return { ok: true, songID: id, status };
}
