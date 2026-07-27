# Протокол TaskFlow WebSocket v1

> Источник: [websocket-protocol.md](../websocket-protocol.md). Английская документация является канонической. При изменении поведения, команд или контрактов необходимо проверить обе языковые версии.

Endpoint: `GET /api/v1/ws` с `Authorization: Bearer <access JWT>` во время HTTP upgrade.
Токены в URL отклоняются. Сервер закрывает соединение после истечения или отзыва сессии.

Каждый frame — один UTF-8 JSON object со стабильным envelope:

```json
{
  "v": 1,
  "kind": "event",
  "event_id": "8a108a76-b7dd-4dc7-9ad4-7c59260cc310",
  "sequence_id": 42,
  "type": "tasks.update",
  "project_id": "a31166a7-1f23-4a22-abf6-ee22ce9dd126",
  "entity_id": "919c13de-67c2-4fa0-9575-2d97f48ff878",
  "occurred_at": "2026-07-27T10:00:00Z",
  "payload": {}
}
```

`event_id` стабилен глобально и используется для deduplication. `sequence_id`
монотонно возрастает в потоке одного получателя. Delivery выполняется at least once.

## Client controls

- `{"v":1,"kind":"ack","sequence_id":42}` устойчиво подтверждает события до 42 включительно.
- `{"v":1,"kind":"resume","after_sequence_id":42}` запрашивает сохранённые события строго после 42 до переключения на live delivery.
- `{"v":1,"kind":"ping","nonce":"..."}` получает соответствующий `pong`.

## Server controls

- `ready` сообщает authenticated user и retention boundary.
- `resync_required` означает, что запрошенный sequence больше не хранится; клиент должен перечитать REST state.
- `authorization_changed` немедленно удаляет project subscriptions.
- `slow_consumer` предшествует retryable close code 1013 при переполнении bounded outbound queue.

Неизвестные версии или `kind` являются protocol errors. Acknowledgements монотонны
и идемпотентны; подтверждение выше наибольшего доставленного `sequence_id` отклоняется.
