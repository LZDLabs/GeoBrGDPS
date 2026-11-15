// Chests and daily quests served by the GDPS (same encoding GD expects from
// RobTop's server: XOR + base64, a 5-character prefix and a sha1 check).
import { type Ctx, gdAccount, param } from "../lib/core.ts";
import { SALT_CHESTS, SALT_QUESTS, XOR_CHESTS, XOR_QUESTS, b64Encode, b64urlEncode, clean, decodeChk, safeInt, sha1, xorCipher } from "../lib/gd.ts";

const SMALL_WAIT = 4 * 3600;
const BIG_WAIT = 24 * 3600;

const rand = (min: number, max: number) => min + Math.floor(Math.random() * (max - min + 1));

async function subjectOf(ctx: Ctx): Promise<{ subject: string; accountId: number } | null> {
  const account = await gdAccount(ctx);
  if (account) return { subject: `a:${account.account_id}`, accountId: account.account_id };
  const udid = clean(param(ctx, "udid"), 64);
  return udid ? { subject: `u:${udid}`, accountId: 0 } : null;
}

export async function getGJRewards(ctx: Ctx): Promise<string> {
  const who = await subjectOf(ctx);
  if (!who) return "-1";
  const type = safeInt(param(ctx, "rewardType"), 0, 0, 2);
  const chk = decodeChk(param(ctx, "chk"), XOR_CHESTS);
  await ctx.sql`insert into geobr_gdps.chest_state (subject) values (${who.subject}) on conflict do nothing`;
  const [s] = await ctx.sql`
    select small_count, big_count,
      greatest(0, ${SMALL_WAIT} - coalesce(extract(epoch from (now() - small_at)), ${SMALL_WAIT}))::int as small_left,
      greatest(0, ${BIG_WAIT} - coalesce(extract(epoch from (now() - big_at)), ${BIG_WAIT}))::int as big_left
    from geobr_gdps.chest_state where subject = ${who.subject}`;
  let smallLeft = s.small_left, bigLeft = s.big_left, smallCount = s.small_count, bigCount = s.big_count;
  if (type === 1) {
    if (smallLeft > 0) return "-1";
    smallCount += 1; smallLeft = SMALL_WAIT;
    await ctx.sql`update geobr_gdps.chest_state set small_at = now(), small_count = small_count + 1 where subject = ${who.subject}`;
  } else if (type === 2) {
    if (bigLeft > 0) return "-1";
    bigCount += 1; bigLeft = BIG_WAIT;
    await ctx.sql`update geobr_gdps.chest_state set big_at = now(), big_count = big_count + 1 where subject = ${who.subject}`;
  }
  const small = [rand(200, 400), rand(2, 10), rand(1, 6), rand(0, 1)].join(",");
  const big = [rand(2000, 4000), rand(20, 100), rand(1, 6), rand(1, 2)].join(",");
  const udid = clean(param(ctx, "udid"), 64);
  const plain = ["1", who.accountId, chk, udid, who.accountId, smallLeft, small, smallCount, bigLeft, big, bigCount, type].join(":");
  const encoded = b64urlEncode(xorCipher(plain, XOR_CHESTS));
  return `SaKuJ${encoded}|${await sha1(encoded + SALT_CHESTS)}`;
}

/** Deterministic daily pick: the same 3 quests for everyone on a given UTC day. */
function pickDaily<T>(items: T[], day: number): T[] {
  const out = [...items];
  let seed = day * 2654435761 >>> 0;
  for (let i = out.length - 1; i > 0; i--) {
    seed = (seed * 1103515245 + 12345) >>> 0;
    const j = seed % (i + 1);
    [out[i], out[j]] = [out[j], out[i]];
  }
  return out.slice(0, 3);
}

export async function getGJChallenges(ctx: Ctx): Promise<string> {
  const quests = await ctx.sql`select kind, amount, reward, name from geobr_gdps.quests where enabled order by quest_id`;
  if (quests.length < 3) return "-1";
  const now = ctx.now.getTime() / 1000;
  const day = Math.floor((now - Date.UTC(2000, 11, 17) / 1000) / 86400);
  const firstId = day * 3;
  const next = Date.UTC(ctx.now.getUTCFullYear(), ctx.now.getUTCMonth(), ctx.now.getUTCDate() + 1) / 1000;
  const timeLeft = Math.max(1, Math.floor(next - now));
  const picked = pickDaily(quests, day).map((q, i) => [firstId + i, q.kind, q.amount, q.reward, clean(q.name, 40).replace(/,/g, "")].join(","));
  const account = await gdAccount(ctx);
  const accountId = account?.account_id ?? 0;
  const chk = decodeChk(param(ctx, "chk"), XOR_QUESTS);
  const udid = clean(param(ctx, "udid"), 64);
  const plain = ["SaKuJ", accountId, chk, udid, accountId, timeLeft, ...picked].join(":");
  const encoded = b64Encode(xorCipher(plain, XOR_QUESTS));
  return `SaKuJ${encoded}|${await sha1(encoded + SALT_QUESTS)}`;
}
