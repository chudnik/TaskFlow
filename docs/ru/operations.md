# Руководство по эксплуатации TaskFlow

> Источник: [operations.md](../operations.md). Английская документация является канонической. При изменении поведения, команд или контрактов необходимо проверить обе языковые версии.

## Локальный запуск

Требуются Docker Compose, CMake 3.25+, Conan 2 и компилятор C++20.

```sh
conan install . --output-folder=build/conan/debug --build=missing \
  --settings=build_type=Debug
cmake --preset conan-debug
cmake --build --preset conan-debug
TASKFLOW_JWT_SIGNING_SECRET=local-development-secret-at-least-32-bytes \
  docker compose up --build
```

Перед rollout API/worker явно выполните миграции:

```sh
docker compose run --rm migrations
```

`GET /health/live` подтверждает работу процесса. `GET /health/ready` проверяет
PostgreSQL и совместимость schema version; API остаётся unready, если применённая
версия схемы отличается от требуемой бинарником.

## Конфигурация

Обязательны `TASKFLOW_POSTGRES_DSN`, `TASKFLOW_REDIS_URI` и
`TASKFLOW_JWT_SIGNING_SECRET` длиной не менее 32 байт. Каждый secret поддерживает
соответствующую `_FILE` variable; одновременно задавать обе формы нельзя.

Operational defaults:

- HTTP address/port: `0.0.0.0:8080`;
- PostgreSQL statement timeout: 5 секунд;
- HTTP idle timeout: 30 секунд;
- maximum request body: 1 MiB;
- login/refresh limits: 10/30 за 60 секунд;
- maximum logical connections: 1000.
- worker poll/batch/lease: 500 мс / 16 / 30 секунд;
- границы worker retry: от 250 мс до 30 секунд;
- graceful shutdown deadline: 20 секунд.

TLS завершается на trusted reverse proxy. Там же удаляются client-supplied forwarded
headers. Access/refresh tokens никогда не передаются в URL.

## API и WebSocket

Канонический контракт: [taskflow-v1.json](../../openapi/taskflow-v1.json).
После register/login передавайте `Authorization: Bearer <access-token>`. Для
task mutations используйте актуальное `version`.

Подробности: [протокол WebSocket](websocket-protocol.md). Храните наибольший
обработанный `sequence_id`, подтверждайте его и переподключайтесь через `resume`.
После `resync_required` перечитайте REST state. At-least-once events дедуплицируются по `event_id`.

## Backup и restore

PostgreSQL — источник истины; Redis не требует backup.

```sh
pg_dump --format=custom "$TASKFLOW_POSTGRES_DSN" > taskflow.dump
pg_restore --clean --if-exists --no-owner --dbname="$RESTORE_DSN" taskflow.dump
```

Восстанавливайте в изолированную БД, проверьте schema compatibility/readiness и только
затем переключайте traffic. Сохраняйте audit rows. Перед promotion сверяйте row counts
для users, projects, tasks, audit/outbox, notifications и jobs.

## Восстановление после сбоев

- PostgreSQL outage: readiness падает; остановите mutations и восстановите соединение.
- Redis outage: durable commits продолжаются; gateway/worker опрашивают PostgreSQL и восстанавливают wake-ups.
- Publisher crash: истёкшие outbox leases забираются через `SKIP LOCKED`.
- Worker crash: истёкшие job leases забираются повторно; handlers перепроверяют task state и остаются idempotent.
- Poison event/job: изучите очищенный `last_error` и correlation ID, исправьте причину и reschedule.
- Slow WebSocket client: закрывается с retryable code 1013 и возобновляет delivery из durable notifications.

Бинарники можно откатывать, пока миграции additive. Schema rollback или destructive
migration требует отдельного two-phase change.

## Worker и graceful shutdown

Health check контейнера worker подтверждает, что постоянный цикл работает. Idle
worker остаётся healthy и использует прерываемое bounded ожидание. Истечение
PostgreSQL lease позволяет восстановить jobs и outbox events после сбоя.

При `SIGTERM` API и worker прекращают принимать или арендовать новую работу,
завершают текущую, закрывают WebSocket и выходят до shutdown deadline. Во время
drain API отвечает на business requests ошибкой `503 shutting_down`. Сценарий
`tests/integration/runtime_fault_test.sh` проверяет shutdown и восстановление
dependencies, а `tests/integration/full_runtime_e2e.sh` — публичный end-to-end flow.
