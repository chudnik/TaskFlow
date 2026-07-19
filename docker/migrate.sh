#!/bin/sh
set -eu

migrations_dir=${TASKFLOW_MIGRATIONS_DIR:-/migrations}
lock_id=1413563980
plan=$(mktemp)

cleanup() {
  rm -f "$plan"
}
trap cleanup EXIT INT TERM

cat >"$plan" <<SQL
\set ON_ERROR_STOP on
SELECT pg_advisory_lock(${lock_id});
CREATE TABLE IF NOT EXISTS taskflow_schema_migrations (
  version bigint PRIMARY KEY CHECK (version > 0),
  name text NOT NULL,
  checksum_sha256 text NOT NULL CHECK (length(checksum_sha256) = 64),
  applied_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
SQL

previous=0
count=0
for migration in "$migrations_dir"/V*.sql; do
  [ -f "$migration" ] || continue
  filename=${migration##*/}
  case "$filename" in
    V[0-9][0-9][0-9][0-9]__[A-Za-z0-9_]*.sql) ;;
    *) printf 'Invalid migration filename: %s\n' "$filename" >&2; exit 1 ;;
  esac
  case "$filename" in
    *[!A-Za-z0-9_.]*) printf 'Invalid migration filename: %s\n' "$filename" >&2; exit 1 ;;
  esac

  version_text=${filename#V}
  version_text=${version_text%%__*}
  version=$(printf '%s' "$version_text" | sed 's/^0*//')
  version=${version:-0}
  expected=$((previous + 1))
  if [ "$version" -ne "$expected" ]; then
    printf 'Migration sequence gap: expected V%04d, found %s\n' "$expected" "$filename" >&2
    exit 1
  fi
  previous=$version
  count=$((count + 1))

  checksum=$(sha256sum "$migration" | awk '{print $1}')
  name=${filename#*__}
  name=${name%.sql}
  cat >>"$plan" <<SQL
SELECT EXISTS(SELECT 1 FROM taskflow_schema_migrations WHERE version = ${version}) AS already_applied \gset
\if :already_applied
DO \$\$ BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM taskflow_schema_migrations
    WHERE version = ${version} AND checksum_sha256 = '${checksum}'
  ) THEN
    RAISE EXCEPTION 'checksum mismatch for already applied migration ${filename}';
  END IF;
END \$\$;
\else
BEGIN;
\ir ${migration}
INSERT INTO taskflow_schema_migrations(version, name, checksum_sha256)
VALUES (${version}, '${name}', '${checksum}');
COMMIT;
\endif
SQL
done

cat >>"$plan" <<SQL
SELECT pg_advisory_unlock(${lock_id});
SQL

psql "$TASKFLOW_POSTGRES_DSN" --no-psqlrc --file="$plan"
printf 'TaskFlow migration plan verified: %s files; target schema version: %s\n' "$count" "$previous"
