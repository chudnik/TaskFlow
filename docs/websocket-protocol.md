# TaskFlow WebSocket protocol v1

Endpoint: `GET /api/v1/ws` with `Authorization: Bearer <access JWT>` during the
HTTP upgrade. Tokens in URLs are rejected. The server closes the connection when
the authenticated session expires or is revoked.

Every frame is one UTF-8 JSON object with a stable envelope:

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

`event_id` is globally stable for deduplication. `sequence_id` is monotonically
ordered within one recipient stream. Delivery is at least once.

Client controls:

- `{"v":1,"kind":"ack","sequence_id":42}` durably acknowledges all events
  through 42.
- `{"v":1,"kind":"resume","after_sequence_id":42}` requests retained events
  strictly after 42 before live handoff.
- `{"v":1,"kind":"ping","nonce":"..."}` receives a matching `pong`.

Server controls:

- `ready` reports the authenticated user and retention boundary.
- `resync_required` means the requested sequence is no longer retained; the
  client must reload REST state before resuming.
- `authorization_changed` removes project subscriptions immediately.
- `slow_consumer` precedes retryable close code 1013 when the bounded outbound
  queue is exceeded.

Unknown versions or kinds are protocol errors. Acknowledgements are monotonic
and idempotent; acknowledging beyond the highest delivered sequence is rejected.
