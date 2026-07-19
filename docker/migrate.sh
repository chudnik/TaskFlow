#!/bin/sh
set -eu

applied=0
for migration in /migrations/*.sql; do
  if [ ! -f "$migration" ]; then
    continue
  fi
  psql "$TASKFLOW_POSTGRES_DSN" --set=ON_ERROR_STOP=1 --file="$migration"
  applied=$((applied + 1))
done

printf 'TaskFlow migration files applied: %s\n' "$applied"
