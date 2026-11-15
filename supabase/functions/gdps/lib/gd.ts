// Geometry Dash 2.2 wire formats: key:value strings, URL-safe base64, the
// XOR ciphers and the sha1 "chk" hashes the client verifies.

export const SALT_LEVEL = "xI25fpAapCQg";
export const GJP2_SALT = "mI29fmAnxgTs";
export const XOR_LEVEL_PASSWORD = "26364";
export const XOR_MESSAGE = "14251";

const enc = new TextEncoder();
const dec = new TextDecoder();

async function digestHex(algo: "SHA-1" | "SHA-256", data: string | Uint8Array): Promise<string> {
  const bytes = typeof data === "string" ? enc.encode(data) : data;
  const hash = await crypto.subtle.digest(algo, bytes as Uint8Array<ArrayBuffer>);
  return Array.from(new Uint8Array(hash), (b) => b.toString(16).padStart(2, "0")).join("");
}
export const sha1 = (data: string | Uint8Array) => digestHex("SHA-1", data);
export const sha256 = (data: string | Uint8Array) => digestHex("SHA-256", data);

export function b64urlEncode(data: string | Uint8Array): string {
  const bytes = typeof data === "string" ? enc.encode(data) : data;
  let bin = "";
  for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  return btoa(bin).replaceAll("+", "-").replaceAll("/", "_");
}

export function b64urlDecode(text: string): string {
  try {
    let s = text.replaceAll("-", "+").replaceAll("_", "/").replace(/[^A-Za-z0-9+/=]/g, "");
    s = s.replace(/=+$/, "");
    while (s.length % 4) s += "=";
    const bin = atob(s);
    return dec.decode(Uint8Array.from(bin, (c) => c.charCodeAt(0)));
  } catch {
    return "";
  }
}

export function xorCipher(text: string, key: string): string {
  let out = "";
  for (let i = 0; i < text.length; i++) out += String.fromCharCode(text.charCodeAt(i) ^ key.charCodeAt(i % key.length));
  return out;
}

/** GD's "key:value:key:value" record, with a custom separator when asked. */
export function record(pairs: Array<[string | number, unknown]>, sep = ":"): string {
  return pairs.map(([k, v]) => `${k}${sep}${v ?? ""}`).join(sep);
}

/** Text that goes inside a ':'/'|'/'#'/'~' separated response must not contain them. */
export function clean(text: unknown, max = 255): string {
  return String(text ?? "").replace(/[:|#~\r\n]/g, "").slice(0, max);
}

/** Human age the way GD shows it ("5 minutes", "2 days"). */
export function age(from: Date | string | null | undefined, now = new Date()): string {
  if (!from) return "0 seconds";
  const seconds = Math.max(0, Math.floor((now.getTime() - new Date(from).getTime()) / 1000));
  const units: Array<[string, number]> = [["year", 31536000], ["month", 2592000], ["week", 604800], ["day", 86400], ["hour", 3600], ["minute", 60]];
  for (const [name, size] of units) {
    if (seconds >= size) {
      const n = Math.floor(seconds / size);
      return `${n} ${name}${n === 1 ? "" : "s"}`;
    }
  }
  return `${seconds} second${seconds === 1 ? "" : "s"}`;
}

export const gjp2Of = (password: string) => sha1(password + GJP2_SALT);

/** chk over a page of levels: first/last ID digit + stars + verified coins. */
export function genMulti(levels: Array<{ id: number | string; stars: number; coinsVerified: boolean }>): Promise<string> {
  let s = "";
  for (const l of levels) {
    const id = String(l.id);
    s += id[0] + id[id.length - 1] + String(l.stars) + (l.coinsVerified ? "1" : "0");
  }
  return sha1(s + SALT_LEVEL);
}

/** chk over a level string: 40 characters sampled evenly. */
export function genSolo(levelString: string): Promise<string> {
  const chars = Array(40).fill("a");
  const len = levelString.length;
  if (len > 0) {
    const step = Math.floor(len / 40);
    if (step === 0) chars.fill(levelString[0]);
    else for (let p = 0, k = 0; p < 40 && k < len; p++, k += step) chars[p] = levelString[k];
  }
  return sha1(chars.join("") + SALT_LEVEL);
}

export const genSolo2 = (text: string) => sha1(text + SALT_LEVEL);

/** Stars to GD difficulty fields (key 9 numerator, auto, demon). */
export function difficultyOfStars(stars: number): { difficulty: number; auto: boolean; demon: boolean } {
  switch (stars) {
    case 1: return { difficulty: 50, auto: true, demon: false };
    case 2: return { difficulty: 10, auto: false, demon: false };
    case 3: return { difficulty: 20, auto: false, demon: false };
    case 4: case 5: return { difficulty: 30, auto: false, demon: false };
    case 6: case 7: return { difficulty: 40, auto: false, demon: false };
    case 8: case 9: return { difficulty: 50, auto: false, demon: false };
    case 10: return { difficulty: 50, auto: false, demon: true };
    default: return { difficulty: 0, auto: false, demon: false };
  }
}

/** GD's demon rating 1..5 (easy..extreme) to key 43. */
export const DEMON_KEY43 = [0, 3, 4, 0, 5, 6];

/** Level password as GD 2.2 expects it in key 27. */
export function encodeLevelPassword(password: string): string {
  if (!password || password === "0") return "0";
  return b64urlEncode(xorCipher(password, XOR_LEVEL_PASSWORD));
}

export function safeInt(value: unknown, fallback = 0, min = -2147483648, max = 2147483647): number {
  const n = typeof value === "number" ? value : Number.parseInt(String(value ?? ""), 10);
  if (!Number.isFinite(n)) return fallback;
  return Math.min(max, Math.max(min, Math.trunc(n)));
}

/** "1,2,3" -> [1,2,3] (only positive ints, deduplicated, capped). */
export function idList(text: unknown, cap = 100): number[] {
  const out = new Set<number>();
  for (const part of String(text ?? "").split(",")) {
    const n = Number.parseInt(part, 10);
    if (Number.isFinite(n) && n > 0) out.add(n);
    if (out.size >= cap) break;
  }
  return [...out];
}

/** Seconds from `now` to the next UTC midnight / next Monday 00:00 UTC. */
export function secondsToReset(kind: number, now = new Date()): number {
  const next = new Date(Date.UTC(now.getUTCFullYear(), now.getUTCMonth(), now.getUTCDate() + 1));
  if (kind === 1) {
    const day = now.getUTCDay(); // 0 Sunday
    const add = ((8 - day) % 7) || 7;
    next.setTime(Date.UTC(now.getUTCFullYear(), now.getUTCMonth(), now.getUTCDate() + add));
  }
  return Math.max(1, Math.floor((next.getTime() - now.getTime()) / 1000));
}

export const SALT_QUESTS = "oC36fpYaPtdg";
export const SALT_CHESTS = "pC26fpYaQCtg";
export const XOR_CHESTS = "59182";
export const XOR_QUESTS = "19847";

/** Standard (not URL-safe) base64 of a binary string. */
export function b64Encode(text: string): string {
  return btoa(text);
}

/** "abcde" + base64(xor(value)) as GD sends chk for chests/quests -> value. */
export function decodeChk(chk: string, key: string): string {
  if (chk.length <= 5) return "";
  try {
    let s = chk.slice(5).replaceAll("-", "+").replaceAll("_", "/").replace(/[^A-Za-z0-9+/=]/g, "");
    while (s.length % 4) s += "=";
    return xorCipher(atob(s), key).replace(/[^0-9A-Za-z]/g, "").slice(0, 32);
  } catch {
    return "";
  }
}

/** chk over a page of map packs: first/last ID digit + stars + coins. */
export function genPack(packs: Array<{ id: number | string; stars: number; coins: number }>): Promise<string> {
  let s = "";
  for (const p of packs) {
    const id = String(p.id);
    s += id[0] + id[id.length - 1] + String(p.stars) + String(p.coins);
  }
  return sha1(s + SALT_LEVEL);
}
