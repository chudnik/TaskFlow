CREATE FUNCTION emit_business_audit_outbox() RETURNS trigger
LANGUAGE plpgsql AS $$
DECLARE
  source jsonb := CASE WHEN TG_OP = 'DELETE' THEN to_jsonb(OLD) ELSE to_jsonb(NEW) END;
  before_value jsonb := CASE WHEN TG_OP IN ('UPDATE', 'DELETE') THEN to_jsonb(OLD) ELSE NULL END;
  after_value jsonb := CASE WHEN TG_OP IN ('INSERT', 'UPDATE') THEN to_jsonb(NEW) ELSE NULL END;
  project_value uuid;
  task_value uuid;
  entity_value uuid;
  actor_value uuid;
  event_value uuid := gen_random_uuid();
  event_name text := TG_TABLE_NAME || '.' || lower(TG_OP);
  entity_name text := rtrim(TG_TABLE_NAME, 's');
  correlation_value text := COALESCE(NULLIF(current_setting('taskflow.correlation_id', true), ''),
                                     'database-trigger');
BEGIN
  IF TG_TABLE_NAME = 'projects' THEN
    project_value := (source->>'id')::uuid;
    entity_value := project_value;
    actor_value := NULLIF(COALESCE(current_setting('taskflow.actor_id', true),
                                   source->>'created_by'), '')::uuid;
  ELSIF TG_TABLE_NAME = 'project_members' THEN
    project_value := (source->>'project_id')::uuid;
    entity_value := (source->>'user_id')::uuid;
    actor_value := NULLIF(COALESCE(current_setting('taskflow.actor_id', true),
                                   source->>'user_id'), '')::uuid;
  ELSIF TG_TABLE_NAME = 'tasks' THEN
    project_value := (source->>'project_id')::uuid;
    task_value := (source->>'id')::uuid;
    entity_value := task_value;
    actor_value := NULLIF(COALESCE(current_setting('taskflow.actor_id', true),
                                   source->>'creator_id'), '')::uuid;
  ELSIF TG_TABLE_NAME = 'comments' THEN
    task_value := (source->>'task_id')::uuid;
    SELECT project_id INTO project_value FROM tasks WHERE id = task_value;
    entity_value := (source->>'id')::uuid;
    actor_value := NULLIF(COALESCE(current_setting('taskflow.actor_id', true),
                                   source->>'author_id'), '')::uuid;
  END IF;

  before_value := before_value - ARRAY['password_hash', 'refresh_token_hash', 'token', 'secret'];
  after_value := after_value - ARRAY['password_hash', 'refresh_token_hash', 'token', 'secret'];

  INSERT INTO audit_events(event_id, project_id, task_id, actor_type, actor_user_id,
                           event_type, entity_type, entity_id, before_data, after_data,
                           correlation_id)
  VALUES(event_value, project_value, task_value,
         CASE WHEN actor_value IS NULL THEN 'system' ELSE 'user' END,
         actor_value, event_name, entity_name, entity_value,
         before_value, after_value, correlation_value);

  INSERT INTO outbox_events(event_id, project_id, aggregate_type, aggregate_id,
                            event_type, payload, correlation_id)
  VALUES(event_value, project_value, entity_name, entity_value, event_name,
         jsonb_build_object('event_id', event_value, 'project_id', project_value,
                            'task_id', task_value, 'entity_id', entity_value),
         correlation_value);
  RETURN NULL;
END;
$$;

CREATE TRIGGER projects_audit_outbox
AFTER INSERT OR UPDATE OR DELETE ON projects
FOR EACH ROW EXECUTE FUNCTION emit_business_audit_outbox();

CREATE TRIGGER project_members_audit_outbox
AFTER INSERT OR UPDATE OR DELETE ON project_members
FOR EACH ROW EXECUTE FUNCTION emit_business_audit_outbox();

CREATE TRIGGER tasks_audit_outbox
AFTER INSERT OR UPDATE OR DELETE ON tasks
FOR EACH ROW EXECUTE FUNCTION emit_business_audit_outbox();

CREATE TRIGGER comments_audit_outbox
AFTER INSERT OR UPDATE OR DELETE ON comments
FOR EACH ROW EXECUTE FUNCTION emit_business_audit_outbox();
