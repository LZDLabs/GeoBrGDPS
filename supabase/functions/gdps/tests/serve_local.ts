// Serves the GDPS on http://127.0.0.1:8787/gdps with the local test database
// (memory Storage). For running the live scripts against local code.
import postgres from "postgres";
import { createHandler } from "../app.ts";
import { MemoryStore } from "../lib/storage.ts";

const sql = postgres({ host: "127.0.0.1", port: 54329, database: Deno.env.get("GDPS_TEST_DB") ?? "gdps_test", username: "postgres", prepare: false, max: 4, onnotice: () => {} });
const handler = createHandler({
  sql, store: new MemoryStore(),
  env: { supabaseUrl: "http://127.0.0.1:8787", serviceKey: "", functionUrl: "http://127.0.0.1:8787/gdps", songsBucket: "geobr-gdps-songs", savesBucket: "geobr-gdps-saves" },
});
Deno.serve({ port: 8787, hostname: "127.0.0.1" }, handler);
