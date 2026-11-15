// GDPS comment commands: staff type "!rate 5" in a level's comments (or
// "!lbban Nome" in any comment / profile post) and the server runs the matching
// admin action. Permissions are the same as in the admin panel; players who
// type "!" just post a normal comment. GD 2.2 shows "temp_<s>_<text>" replies.
import { type Account, type Ctx, can, grantsOf } from "../lib/core.ts";
import { clean, safeInt } from "../lib/gd.ts";
import { runAction } from "./admin.ts";

export const reply = (text: string) => `temp_0_${clean(text, 140).replace(/_/g, " ")}`;

const HELP = "Comandos (/ ou !): /daily, /weekly (entram na fila), /event 500 diamantes 1000 orbs, /rate N, /unrate, /feature, /epic, /legendary, /mythic, /coins, /block, /delete, /ban NOME, /mute NOME horas, /lbban NOME, /lbunban";

async function accountByName(ctx: Ctx, name: string): Promise<number | null> {
  const n = name.trim();
  if (!n) return null;
  const [row] = /^\d+$/.test(n)
    ? await ctx.sql`select account_id::int as id from geobr_gdps.accounts where account_id = ${Number(n)}`
    : await ctx.sql`select account_id::int as id from geobr_gdps.accounts where lower(username) = lower(${n})`;
  return row?.id ?? null;
}

/** Runs a "!" command. Returns null when the text is not a command for this account (post it normally). */
export async function runCommand(ctx: Ctx, account: Account, text: string, levelId: number | null): Promise<string | null> {
  const trimmed = text.trim();
  if (!trimmed.startsWith("!") && !trimmed.startsWith("/")) return null;
  const g = await grantsOf(ctx, account.account_id);
  if (!can(g, "admin.panel")) return null;
  const [rawCmd, ...args] = trimmed.slice(1).split(/\s+/);
  const cmd = (rawCmd ?? "").toLowerCase();
  const rest = (from: number) => args.slice(from).join(" ");
  // deno-lint-ignore no-explicit-any
  const done = (r: Record<string, any>, msg: string) => reply(r.ok ? msg : `Erro: ${r.error ?? "falhou"}`);
  // deno-lint-ignore no-explicit-any
  const act = async (name: string, body: Record<string, unknown>): Promise<Record<string, any>> => await runAction(ctx, account.account_id, name, body);

  if (cmd === "help" || cmd === "ajuda") return reply(HELP);

  const levelCommands = new Set(["rate", "unrate", "feature", "unfeature", "epic", "legendary", "mythic", "unepic", "coins", "verifycoins", "uncoins",
    "demon", "daily", "weekly", "event", "block", "unblock", "delete"]);
  if (levelCommands.has(cmd)) {
    if (!levelId) return reply("Esse comando so funciona nos comentarios de um level.");
    const [lvl] = await ctx.sql`select stars, coins_verified, demon_diff from geobr_gdps.levels where level_id = ${levelId} and not deleted`;
    if (!lvl && cmd !== "delete") return reply("Level nao encontrado.");
    switch (cmd) {
      case "rate": {
        const stars = safeInt(args[0], -1, -1, 10);
        if (stars < 0) return reply("Use: !rate ESTRELAS (0 a 10) [demon 1-5]");
        const body: Record<string, unknown> = { levelID: levelId, stars };
        if (args[1]) body.demonDiff = safeInt(args[1], 3, 1, 5);
        return done(await act("levels.rate", body), stars ? `Level avaliado com ${stars} estrela(s).` : "Rate removido.");
      }
      case "unrate": return done(await act("levels.rate", { levelID: levelId, stars: 0 }), "Rate removido.");
      case "feature": return done(await act("levels.feature", { levelID: levelId, feature: true, epic: 0 }), "Level em Featured.");
      case "unfeature": return done(await act("levels.feature", { levelID: levelId, feature: false, epic: 0 }), "Featured removido.");
      case "epic": return done(await act("levels.feature", { levelID: levelId, feature: true, epic: 1 }), "Level Epic.");
      case "legendary": return done(await act("levels.feature", { levelID: levelId, feature: true, epic: 2 }), "Level Legendary.");
      case "mythic": return done(await act("levels.feature", { levelID: levelId, feature: true, epic: 3 }), "Level Mythic.");
      case "unepic": return done(await act("levels.feature", { levelID: levelId, feature: true, epic: 0 }), "Epic removido (continua Featured).");
      case "coins": case "verifycoins":
        return done(await act("levels.rate", { levelID: levelId, stars: lvl.stars, coins: true }), "Moedas verificadas.");
      case "uncoins": return done(await act("levels.rate", { levelID: levelId, stars: lvl.stars, coins: false }), "Moedas sem verificacao.");
      case "demon": {
        if (lvl.stars !== 10) return reply("O level precisa ter rate de 10 estrelas (Demon).");
        return done(await act("levels.rate", { levelID: levelId, stars: 10, demonDiff: safeInt(args[0], 3, 1, 5) }), "Dificuldade de Demon alterada.");
      }
      case "daily": case "weekly": {
        const kind = cmd === "daily" ? 0 : 1;
        const r = await act("events.set", { kind, levelID: levelId });
        if (!r.ok) return done(r, "");
        const when = new Date(r.starts_at).toISOString().slice(0, 10);
        return reply(r.queued ? `Na fila do ${cmd === "daily" ? "Daily" : "Weekly"}: entra em ${when}.` : `${cmd === "daily" ? "Daily de hoje" : "Weekly desta semana"} definido.`);
      }
      case "event": {
        const r = await act("events.set", { kind: 2, levelID: levelId, rewards: rest(0) });
        return done(r, `Evento definido (recompensas ${r.rewards ?? ""}).`);
      }
      case "block": return done(await act("levels.block", { levelID: levelId, reason: rest(0) }), "Level bloqueado.");
      case "unblock": return done(await act("levels.unblock", { levelID: levelId }), "Level desbloqueado.");
      case "delete": return done(await act("levels.delete", { levelID: levelId, reason: rest(0) }), "Level apagado.");
    }
  }

  const userCommands = new Set(["ban", "unban", "mute", "unmute", "lbban", "lbunban"]);
  if (userCommands.has(cmd)) {
    const target = await accountByName(ctx, args[0] ?? "");
    if (!target) return reply("Jogador nao encontrado. Use: !" + cmd + " NOME");
    switch (cmd) {
      case "ban": {
        const days = safeInt(args[1], 0, 0, 3650);
        const reason = /^\d+$/.test(args[1] ?? "") ? rest(2) : rest(1);
        return done(await act("accounts.ban", { target, days: /^\d+$/.test(args[1] ?? "") ? days : 0, reason }), "Jogador banido.");
      }
      case "unban": return done(await act("accounts.unban", { target }), "Banimento removido.");
      case "mute": return done(await act("accounts.mute", { target, hours: safeInt(args[1], 24, 1, 8760), reason: rest(2) }), "Comentarios bloqueados.");
      case "unmute": return done(await act("accounts.unmute", { target }), "Comentarios liberados.");
      case "lbban": return done(await act("lb.ban", { target, reason: rest(1) }), "Jogador fora do ranking.");
      case "lbunban": return done(await act("lb.unban", { target }), "Jogador de volta ao ranking.");
    }
  }
  return reply("Comando desconhecido. Use !help");
}

/** "temp_<seconds>_<reason>" when the account is muted, else null. */
export async function muteReply(ctx: Ctx, accountId: number): Promise<string | null> {
  const [row] = await ctx.sql`
    select greatest(0, extract(epoch from (comment_ban_until - now())))::int as left, comment_ban_reason as reason
    from geobr_gdps.accounts where account_id = ${accountId} and comment_ban_until > now()`;
  if (!row) return null;
  return `temp_${row.left}_${clean(row.reason || "Sem motivo", 100).replace(/_/g, " ")}`;
}
