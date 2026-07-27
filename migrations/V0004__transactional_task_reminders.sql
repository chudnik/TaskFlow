CREATE FUNCTION sync_task_reminder_jobs() RETURNS trigger
LANGUAGE plpgsql AS $$
DECLARE
  key_prefix text := 'task:' || NEW.id::text;
  correlation_value text := COALESCE(NULLIF(current_setting('taskflow.correlation_id', true), ''),
                                     'database-trigger');
  offset_minutes integer :=
    COALESCE(NULLIF(current_setting('taskflow.reminder_offset_minutes', true), '')::integer, 15);
  reminder_payload jsonb;
BEGIN
  UPDATE jobs
     SET status = 'cancelled',
         completed_at = clock_timestamp(),
         lease_owner = NULL,
         lease_expires_at = NULL,
         updated_at = clock_timestamp()
   WHERE business_key IN (key_prefix || ':pre', key_prefix || ':overdue')
     AND status IN ('pending', 'running');

  IF NEW.deleted_at IS NOT NULL OR NEW.assignee_id IS NULL OR NEW.deadline_at IS NULL OR
     NEW.status IN ('done', 'cancelled') THEN
    RETURN NULL;
  END IF;

  reminder_payload := jsonb_build_object(
    'task_id', NEW.id,
    'version', NEW.version,
    'assignee_id', NEW.assignee_id,
    'deadline_at', NEW.deadline_at
  );

  INSERT INTO jobs(id, business_key, job_type, payload, scheduled_at, max_attempts, correlation_id)
  VALUES
    (gen_random_uuid(), key_prefix || ':pre', 'task.pre_deadline', reminder_payload,
     NEW.deadline_at - make_interval(mins => offset_minutes), 5, correlation_value),
    (gen_random_uuid(), key_prefix || ':overdue', 'task.overdue', reminder_payload,
     NEW.deadline_at, 5, correlation_value)
  ON CONFLICT (business_key) DO UPDATE
    SET job_type = EXCLUDED.job_type,
        payload = EXCLUDED.payload,
        scheduled_at = EXCLUDED.scheduled_at,
        max_attempts = EXCLUDED.max_attempts,
        correlation_id = EXCLUDED.correlation_id,
        status = 'pending',
        attempt = 0,
        lease_owner = NULL,
        lease_expires_at = NULL,
        completed_at = NULL,
        last_error = NULL,
        updated_at = clock_timestamp();
  RETURN NULL;
END;
$$;

CREATE TRIGGER tasks_sync_reminder_jobs
AFTER INSERT OR UPDATE OF assignee_id, deadline_at, status, deleted_at ON tasks
FOR EACH ROW EXECUTE FUNCTION sync_task_reminder_jobs();
