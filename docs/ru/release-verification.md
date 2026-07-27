# Проверка релиза

> Источник: [release-verification.md](../release-verification.md). Английская документация является канонической. При изменении поведения, команд или контрактов необходимо проверить обе языковые версии.

## Первая воспроизводимая конфигурация

- Release: `0.1.0`
- Verified: 2026-07-28
- Language: C++20
- Build: preset CMake `ci-release` с warnings-as-errors
- Dependencies: политика Conan 2.30.0 из `conanfile.py`
- Runtime images: Ubuntu 24.04; PostgreSQL 17.10; Redis 7.4.9
- Database schema: version 5 (`V0001`–`V0005`)
- API contract: OpenAPI 3.1, `/api/v1`

## Доказательства

Release candidate собран из исходников внутри Docker Compose и выполнен с реальными
PostgreSQL и Redis:

```text
93/93 dependency-backed unit/contract CTest tests passed
6/6 dependency-free developer bootstrap tests passed
18/18 PostgreSQL integration tests passed
112/112 AddressSanitizer CTest checks passed
112/112 UndefinedBehaviorSanitizer CTest checks passed
Docker Compose repository-backed HTTP scenario passed
Docker Compose WebSocket runtime scenario passed
PostgreSQL/Redis outage and process-restart recovery passed
API and worker SIGTERM graceful-shutdown checks passed
OpenApiContractTest.IsValidJsonAndCoversVersionedResources passed
OpenSpec strict validation passed
clang-format check passed
Russian documentation and checker tests passed
```

Compose E2E покрывает registration/login, membership, task/comment lifecycle,
transactional outbox, авторизованную WebSocket delivery, ordered notification
replay/acknowledgement, deadline jobs, удаление membership, correlation, rollback,
restart и восстановление PostgreSQL/Redis.

## Известные ограничения

- Redis Pub/Sub ускоряет delivery; PostgreSQL остаётся durable replay source.
- Delivery выполняется at-least-once, клиент обязан дедуплицировать по `event_id`.
- Audit/notification tables используют retention и indexes, но не partitioning.
- TLS termination и trusted proxy enforcement находятся на deployment edge.
- Локальная цель `tidy` требует установленный `clang-tidy`; CI устанавливает инструмент.
- Runtime является backend-only release; browser frontend не входит в scope этого
  релиза.

Конфигурация подходит как воспроизводимый backend release и integration baseline.
Production deployment всё ещё требует проверки TLS, secret management, observability,
backup и capacity для конкретного окружения.
