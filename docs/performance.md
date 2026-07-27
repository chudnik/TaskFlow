# Query-plan smoke test

The initial schema has partial composite indexes matching the three hot polling
paths:

- `tasks_project_created_idx` for authorized project task pages;
- `outbox_events_claim_idx` plus `outbox_events_locked_idx` for publisher claims;
- `jobs_due_idx` plus `jobs_lease_recovery_idx` for worker leasing.

Run the repeatable smoke test against a populated integration database:

```sh
psql "$TASKFLOW_TEST_POSTGRES_DSN" \
  --set=project_id=00000000-0000-0000-0000-000000000001 \
  --file=tests/integration/performance_smoke.sql
```

The script disables sequential scans to prove each predicate has an eligible
index, uses `EXPLAIN (ANALYZE, BUFFERS, FORMAT JSON)`, caps every page at 100,
and fails if any representative query exceeds the five-second smoke budget.
Production plan review must use representative cardinalities because an empty
table can legitimately choose a sequential scan.
