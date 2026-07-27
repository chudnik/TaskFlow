# Smoke-тест query plan

> Источник: [performance.md](../performance.md). Английская документация является канонической. При изменении поведения, команд или контрактов необходимо проверить обе языковые версии.

Начальная схема содержит partial composite indexes для трёх горячих polling paths:

- `tasks_project_created_idx` для авторизованных project task pages;
- `outbox_events_claim_idx` и `outbox_events_locked_idx` для publisher claims;
- `jobs_due_idx` и `jobs_lease_recovery_idx` для worker leasing.

Повторяемый smoke-тест на заполненной integration database:

```sh
psql "$TASKFLOW_TEST_POSTGRES_DSN" \
  --set=project_id=00000000-0000-0000-0000-000000000001 \
  --file=tests/integration/performance_smoke.sql
```

Скрипт отключает sequential scans, чтобы подтвердить наличие подходящего index,
использует `EXPLAIN (ANALYZE, BUFFERS, FORMAT JSON)`, ограничивает page размером 100
и падает, если representative query превышает пять секунд. Production review должен
использовать representative cardinalities: на пустой таблице PostgreSQL может обоснованно
выбрать sequential scan.
