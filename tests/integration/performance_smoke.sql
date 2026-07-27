\set ON_ERROR_STOP on

BEGIN;
SET LOCAL statement_timeout = '5s';
SET LOCAL enable_seqscan = off;

EXPLAIN (ANALYZE, BUFFERS, FORMAT JSON)
SELECT id FROM tasks
WHERE project_id = :'project_id'::uuid AND deleted_at IS NULL
ORDER BY created_at DESC, id DESC LIMIT 100;

EXPLAIN (ANALYZE, BUFFERS, FORMAT JSON)
SELECT event_id FROM outbox_events
WHERE processed_at IS NULL AND available_at <= clock_timestamp()
  AND (locked_until IS NULL OR locked_until < clock_timestamp())
ORDER BY available_at, occurred_at, event_id LIMIT 100
FOR UPDATE SKIP LOCKED;

EXPLAIN (ANALYZE, BUFFERS, FORMAT JSON)
SELECT id FROM jobs
WHERE status = 'pending' AND scheduled_at <= clock_timestamp()
ORDER BY scheduled_at, id LIMIT 100
FOR UPDATE SKIP LOCKED;

ROLLBACK;
