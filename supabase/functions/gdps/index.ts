// GeoBR GDPS — Geometry Dash private server on Supabase.
// Deployed with verify_jwt = false: GD does not send Supabase tokens, so every
// endpoint authenticates the GD account itself (accountID + gjp2, bcrypt).
import postgres from "postgres";
import { createHandler } from "./app.ts";
import { SupabaseStore } from "./lib/storage.ts";

const supabaseUrl = Deno.env.get("SUPABASE_URL") ?? "";
// Server key from the function environment only (never shipped to the game).
function serverKey(): string {
  const legacy = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY");
  if (legacy) return legacy;
  try {
    const keys = JSON.parse(Deno.env.get("SUPABASE_SECRET_KEYS") ?? "{}");
    return String(keys.default ?? Object.values(keys)[0] ?? "");
  } catch {
    return "";
  }
}
const serviceKey = serverKey();
const sql = postgres(Deno.env.get("SUPABASE_DB_URL") ?? "", {
  prepare: false,
  max: 4,
  idle_timeout: 20,
  connect_timeout: 10,
  onnotice: () => {},
});

Deno.serve(createHandler({
  sql,
  store: new SupabaseStore(supabaseUrl, serviceKey),
  env: {
    supabaseUrl,
    serviceKey,
    functionUrl: `${supabaseUrl}/functions/v1/gdps`,
    songsBucket: "geobr-gdps-songs",
    savesBucket: "geobr-gdps-saves",
  },
}));
