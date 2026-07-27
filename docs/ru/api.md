# Использование TaskFlow API

> Источники: [README.md](../../README.md) и [OpenAPI 3.1](../../openapi/taskflow-v1.json). Английские источники являются каноническими. При изменении поведения, команд или контрактов необходимо проверить обе языковые версии.

Базовый prefix: `/api/v1`. Защищённые запросы используют:

```http
Authorization: Bearer <access-token>
Content-Type: application/json
```

## Аутентификация

Регистрация:

```http
POST /api/v1/auth/register

{"email":"user@example.com","password":"correct horse battery"}
```

Успех возвращает HTTP 201. Повторный email возвращает HTTP 409.

Вход выполняется через `POST /api/v1/auth/login`. Refresh token передаётся в
`POST /api/v1/auth/refresh`, а завершение сессии — в `POST /api/v1/auth/logout`.
Access JWT нельзя передавать в URL.

## Проекты и участники

- `GET /api/v1/projects` — видимые проекты;
- `POST /api/v1/projects` — создать проект;
- `GET|PUT|DELETE /api/v1/projects/{projectId}` — чтение, изменение, архивирование;
- `GET|POST /api/v1/projects/{projectId}/members` — список и добавление;
- `PATCH|DELETE /api/v1/projects/{projectId}/members/{userId}` — роль или удаление.

Создатель проекта становится `owner`. Проект всегда должен иметь хотя бы одного `owner`.
Скрытый проект для обычного non-member возвращает HTTP 404.

## Задачи

`GET /api/v1/tasks` требует `project_id` и поддерживает `status`, `priority`,
`assignee_id`, `creator_id`, `deadline_from`, `deadline_to`, `overdue`, `title`,
`sort`, `cursor`, `page_size`. Максимальный `page_size` равен 100.

Создание:

```http
POST /api/v1/tasks

{"project_id":"<uuid>","title":"Подготовить релиз","priority":"high","deadline_at":"2026-07-28T23:00:00Z"}
```

Изменения используют актуальное поле `version`; устаревшая версия возвращает HTTP 409.
Дополнительные операции:

- `PATCH /api/v1/tasks/{taskId}/status`;
- `PATCH /api/v1/tasks/{taskId}/assignee`;
- `DELETE /api/v1/tasks/{taskId}`.

Допустимые значения статуса: `todo`, `in_progress`, `done`, `cancelled`.

## Комментарии и история

- `GET|POST /api/v1/tasks/{taskId}/comments`;
- `PATCH|DELETE /api/v1/comments/{commentId}`;
- `GET /api/v1/projects/{projectId}/history`;
- `GET /api/v1/tasks/{taskId}/history`.

Комментарии — plain text до 10 000 Unicode characters. History и list endpoints
используют cursor pagination. Публичная ошибка имеет стабильный envelope:

```json
{"error":{"code":"not_found","message":"resource not found","details":[],"request_id":"..."}}
```

## Health endpoints

- `GET /health/live` не зависит от optional dependencies;
- `GET /health/ready` проверяет PostgreSQL и совместимость schema version.
