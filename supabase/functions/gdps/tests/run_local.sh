#!/usr/bin/env bash
# Runs the GDPS server tests against a throwaway local Postgres database.
# Usage: tests/run_local.sh   (needs psql/Postgres on $PGHOST:$PGPORT and deno)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../../.." && pwd)"
export PGHOST="${PGHOST:-/tmp}" PGPORT="${PGPORT:-54329}" PGUSER="${PGUSER:-postgres}"
psql -q -c "drop database if exists gdps_test" -c "create database gdps_test" >/dev/null
psql -q -d gdps_test -v ON_ERROR_STOP=1 -f "$here/scaffold.sql" >/dev/null
for m in 20261108120000_geobr_gdps 20261113120000_geobr_gdps_v2; do psql -q -d gdps_test -v ON_ERROR_STOP=1 -f "$root/supabase/migrations/$m.sql" >/dev/null 2>&1; done
# The migration must be safe to run twice.
for m in 20261108120000_geobr_gdps 20261113120000_geobr_gdps_v2; do psql -q -d gdps_test -v ON_ERROR_STOP=1 -f "$root/supabase/migrations/$m.sql" >/dev/null 2>&1; done
cd "$here/.."
GDPS_TEST_DB=gdps_test "${DENO:-deno}" test --allow-net --allow-env --allow-read tests/gdps_test.ts "$@"
