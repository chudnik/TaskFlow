# TaskFlow operations guide

## Local setup

Requirements are Docker Compose, CMake 3.25+, Conan 2, and a C++20 compiler.

```sh
conan install . --output-folder=build/conan/debug --build=missing \
  --settings=build_type=Debug
cmake --preset conan-debug
cmake --build --preset conan-debug
TASKFLOW_JWT_SIGNING_SECRET=local-development-secret-at-least-32-bytes \
  docker compose up --build
```

Run migrations explicitly before API/worker rollout:

```sh
docker compose run --rm migrations
```

The API stays unready when the applied schema version differs from the binary's
required version.

## Configuration

Required values are `TASKFLOW_POSTGRES_DSN`, `TASKFLOW_REDIS_URI`, and a
32-byte-or-longer `TASKFLOW_JWT_SIGNING_SECRET`. Each secret accepts a
corresponding `_FILE` variable. Do not set both forms.

Operational defaults:

- HTTP address/port: `0.0.0.0:8080`;
- PostgreSQL statement timeout: 5 seconds;
- HTTP idle timeout: 30 seconds;
- maximum request body: 1 MiB;
- login/refresh limits: 10/30 per 60 seconds;
- maximum logical connections: 1000.

Terminate TLS at a trusted reverse proxy. Strip client-supplied forwarded
headers there and never pass access/refresh tokens in URLs.

## API and WebSocket use

The canonical contract is [taskflow-v1.json](../openapi/taskflow-v1.json).
Register/login, send `Authorization: Bearer <access-token>`, create a project,
add members, then use optimistic task `version` values for mutations.

WebSocket protocol details are in [websocket-protocol.md](websocket-protocol.md).
Persist the highest processed `sequence_id`, acknowledge it, and reconnect with
`resume`. On `resync_required`, reload REST state and begin from the new
retention boundary. Deduplicate at-least-once events by `event_id`.

## Backup and restore

PostgreSQL is the source of truth; Redis does not require backup.

```sh
pg_dump --format=custom "$TASKFLOW_POSTGRES_DSN" > taskflow.dump
pg_restore --clean --if-exists --no-owner --dbname="$RESTORE_DSN" taskflow.dump
```

Restore into an isolated database, run schema compatibility/readiness checks,
then switch traffic. Preserve audit rows. Validate row counts for users,
projects, tasks, audit/outbox, notifications, and jobs before promotion.

## Failure recovery

- PostgreSQL outage: readiness fails; stop mutations and restore connectivity.
- Redis outage: durable commits continue; gateways/workers poll PostgreSQL and
  recover wake-ups after Redis returns.
- Publisher crash: expired outbox leases are reclaimed with `SKIP LOCKED`.
- Worker crash: expired job leases are reclaimed; handlers revalidate current
  task state and remain idempotent.
- Poison event/job: inspect sanitized `last_error` and correlation ID; fix the
  cause, then reschedule. Never edit immutable audit history.
- Slow WebSocket client: it is closed with retryable code 1013 and resumes from
  durable notifications.

Roll application binaries back freely while migrations remain additive. Schema
rollback or destructive migration requires a separate two-phase change.
