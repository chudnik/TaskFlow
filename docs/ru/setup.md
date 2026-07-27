# Установка и разработка

> Источник: [README.md](../../README.md). Английская документация является канонической. При изменении поведения, команд или контрактов необходимо проверить обе языковые версии.

## Требования

- CMake 3.25+;
- Conan 2;
- компилятор с поддержкой C++20;
- Docker Compose для PostgreSQL, Redis и контейнерных тестов.

## Быстрая bootstrap-сборка

Этот preset проверяет граф целей без загрузки сторонних пакетов:

```sh
cmake --preset developer
cmake --build --preset developer
ctest --preset developer
```

## Полная сборка с Conan

Один раз создайте профиль, установите зафиксированные зависимости и соберите Debug-конфигурацию:

```sh
conan profile detect --force
conan install . --output-folder=build/conan/debug --build=missing -s build_type=Debug -s compiler.cppstd=20
cmake --preset conan-debug
cmake --build --preset conan-debug
ctest --preset conan-debug
```

Release CI использует `build/conan/release`, `build_type=Release` и preset `ci-release`.
Цели качества: `format`, `format-check`, `tidy`. Sanitizer presets: `asan`, `ubsan`.

## Docker Compose

Задайте development-only JWT secret длиной не менее 32 байт:

```sh
export TASKFLOW_JWT_SIGNING_SECRET='replace-with-a-random-development-secret'
docker compose up --build
```

Состав стека: API, worker, PostgreSQL 17, временный Redis 7 и одноразовый сервис `migrations`.
Данные PostgreSQL находятся в volume `postgres-data`; Redis считается восстанавливаемым.

Миграции можно запустить явно:

```sh
docker compose run --rm migrations
```

Полный контейнерный набор тестов:

```sh
docker compose --profile test run --build --rm integration-tests
```

## Конфигурация

Обязательные значения:

- `TASKFLOW_POSTGRES_DSN`;
- `TASKFLOW_REDIS_URI`;
- `TASKFLOW_JWT_SIGNING_SECRET` длиной не менее 32 байт.

Дополнительные настройки: `TASKFLOW_JWT_ISSUER`, `TASKFLOW_JWT_AUDIENCE`,
`TASKFLOW_HTTP_ADDRESS`, `TASKFLOW_HTTP_PORT`, `TASKFLOW_LOG_LEVEL`.

Каждое значение можно читать из mounted file через вариант `_FILE`, например:

```sh
TASKFLOW_JWT_SIGNING_SECRET_FILE=/run/secrets/taskflow-jwt
```

Одновременное задание обычной и `_FILE` формы является ошибкой. Startup diagnostics
скрывают signing secrets и connection strings.
