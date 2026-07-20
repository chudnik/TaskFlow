\set ON_ERROR_STOP on

DO $$
DECLARE
  missing_tables text[];
BEGIN
  SELECT array_agg(expected.name ORDER BY expected.name)
    INTO missing_tables
  FROM unnest(ARRAY[
    'users', 'sessions', 'projects', 'project_members', 'tasks', 'comments',
    'audit_events', 'outbox_events', 'notification_events', 'jobs'
  ]) AS expected(name)
  WHERE to_regclass('public.' || expected.name) IS NULL;

  IF missing_tables IS NOT NULL THEN
    RAISE EXCEPTION 'missing initial-schema tables: %', missing_tables;
  END IF;

  IF (SELECT max(version) FROM taskflow_schema_migrations) <> 1 THEN
    RAISE EXCEPTION 'expected schema version 2';
  END IF;
END;
$$;

BEGIN;

INSERT INTO users(id, email, password_hash)
VALUES
  ('00000000-0000-0000-0000-000000000001', 'owner@example.test', 'argon2-test-hash'),
  ('00000000-0000-0000-0000-000000000002', 'outsider@example.test', 'argon2-test-hash');

INSERT INTO projects(id, name, created_by)
VALUES ('10000000-0000-0000-0000-000000000001', 'Schema test',
        '00000000-0000-0000-0000-000000000001');

INSERT INTO project_members(project_id, user_id, role)
VALUES ('10000000-0000-0000-0000-000000000001',
        '00000000-0000-0000-0000-000000000001', 'owner');

DO $$
BEGIN
  BEGIN
    INSERT INTO users(id, email, password_hash)
    VALUES ('00000000-0000-0000-0000-000000000003', 'MixedCase@example.test', 'hash');
    RAISE EXCEPTION 'mixed-case email was accepted';
  EXCEPTION WHEN check_violation THEN
    NULL;
  END;

  BEGIN
    INSERT INTO tasks(id, project_id, title, creator_id, assignee_id)
    VALUES ('20000000-0000-0000-0000-000000000001',
            '10000000-0000-0000-0000-000000000001', 'Invalid assignment',
            '00000000-0000-0000-0000-000000000001',
            '00000000-0000-0000-0000-000000000002');
    RAISE EXCEPTION 'non-member assignee was accepted';
  EXCEPTION WHEN foreign_key_violation THEN
    NULL;
  END;

  BEGIN
    INSERT INTO jobs(id, business_key, job_type, payload, status, scheduled_at,
                     max_attempts, correlation_id)
    VALUES ('30000000-0000-0000-0000-000000000001', 'invalid-running', 'test', '{}',
            'running', clock_timestamp(), 3, 'schema-test');
    RAISE EXCEPTION 'running job without lease was accepted';
  EXCEPTION WHEN check_violation THEN
    NULL;
  END;
END;
$$;

INSERT INTO tasks(id, project_id, title, creator_id, assignee_id)
VALUES ('20000000-0000-0000-0000-000000000002',
        '10000000-0000-0000-0000-000000000001', 'Valid task',
        '00000000-0000-0000-0000-000000000001',
        '00000000-0000-0000-0000-000000000001');

INSERT INTO audit_events(event_id, project_id, actor_type, actor_user_id, event_type,
                         entity_type, entity_id, after_data, correlation_id)
VALUES ('40000000-0000-0000-0000-000000000001',
        '10000000-0000-0000-0000-000000000001', 'user',
        '00000000-0000-0000-0000-000000000001', 'task.created', 'task',
        '20000000-0000-0000-0000-000000000002', '{}', 'schema-test');

DO $$
BEGIN
  BEGIN
    UPDATE audit_events SET event_type = 'tampered'
    WHERE event_id = '40000000-0000-0000-0000-000000000001';
    RAISE EXCEPTION 'audit event update was accepted';
  EXCEPTION WHEN raise_exception THEN
    IF SQLERRM <> 'audit events are immutable' THEN
      RAISE;
    END IF;
  END;
END;
$$;

ROLLBACK;
