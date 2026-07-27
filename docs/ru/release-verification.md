# Проверка релиза

> Источник: [release-verification.md](../release-verification.md). Английская документация является канонической. При изменении поведения, команд или контрактов необходимо проверить обе языковые версии.

## Первая воспроизводимая конфигурация

- Release: `0.1.0`
- Verified: 2026-07-28
- Language: C++20
- Build: preset CMake `ci-release` с warnings-as-errors
- Dependencies: политика Conan 2.30.0 из `conanfile.py`
- Runtime images: Ubuntu 24.04; PostgreSQL 17.10; Redis 7.4.9
- Database schema: version 3 (`V0001`–`V0003`)
- API contract: OpenAPI 3.1, `/api/v1`

## Доказательства

Release candidate собран из исходников внутри Docker Compose и выполнен с реальными
PostgreSQL и Redis:

```text
92/92 CTest tests passed
integration.DockerComposeEndToEndSmoke.CoversFirstReleaseLifecycle passed
integration.health-endpoints passed
OpenApiContractTest.IsValidJsonAndCoversVersionedResources passed
OpenSpec strict validation passed
clang-format check passed
AddressSanitizer audit/outbox regression passed
```

Compose E2E покрывает registration/login, membership, task/comment lifecycle,
transactional outbox, ordered notification replay/acknowledgement и deadline jobs.

## Известные ограничения

- Redis Pub/Sub ускоряет delivery; PostgreSQL остаётся durable replay source.
- Delivery выполняется at-least-once, клиент обязан дедуплицировать по `event_id`.
- Audit/notification tables используют retention и indexes, но не partitioning.
- TLS termination и trusted proxy enforcement находятся на deployment edge.
- Локальная цель `tidy` требует установленный `clang-tidy`; CI устанавливает инструмент.
- Drogon executable публикует version и health routing. Product controllers проверены
  contract и Compose E2E tests, но полный REST/WebSocket surface необходимо смонтировать
  перед internet-facing deployment.

Конфигурация подходит как воспроизводимый backend-core release и integration baseline,
но не одобрена для публичного production deployment до устранения routing limitation.
