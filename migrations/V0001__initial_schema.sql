CREATE TABLE users (
  id uuid PRIMARY KEY,
  email text NOT NULL CHECK (email = lower(btrim(email)) AND email <> ''),
  password_hash text NOT NULL CHECK (password_hash <> ''),
  global_role text NOT NULL DEFAULT 'user'
    CHECK (global_role IN ('user', 'admin')),
  account_status text NOT NULL DEFAULT 'active'
    CHECK (account_status IN ('active', 'inactive')),
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  updated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);

CREATE UNIQUE INDEX users_email_lower_uidx ON users (lower(email));
CREATE INDEX users_account_status_idx ON users (account_status);

CREATE TABLE sessions (
  id uuid PRIMARY KEY,
  user_id uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  token_family_id uuid NOT NULL,
  refresh_token_hash text NOT NULL CHECK (refresh_token_hash <> ''),
  expires_at timestamptz NOT NULL,
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  last_rotated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  revoked_at timestamptz,
  revoke_reason text,
  CHECK (expires_at > created_at),
  CHECK ((revoked_at IS NULL) = (revoke_reason IS NULL))
);

CREATE UNIQUE INDEX sessions_refresh_token_hash_uidx ON sessions (refresh_token_hash);
CREATE INDEX sessions_user_active_idx ON sessions (user_id, expires_at)
  WHERE revoked_at IS NULL;
CREATE INDEX sessions_token_family_idx ON sessions (token_family_id);

CREATE TABLE projects (
  id uuid PRIMARY KEY,
  name text NOT NULL CHECK (char_length(btrim(name)) BETWEEN 1 AND 200),
  description text NOT NULL DEFAULT '' CHECK (char_length(description) <= 10000),
  created_by uuid NOT NULL REFERENCES users(id),
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  updated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  archived_at timestamptz,
  archived_by uuid REFERENCES users(id),
  CHECK ((archived_at IS NULL) = (archived_by IS NULL))
);

CREATE INDEX projects_created_by_idx ON projects (created_by);
CREATE INDEX projects_active_updated_idx ON projects (updated_at DESC, id)
  WHERE archived_at IS NULL;

CREATE TABLE project_members (
  project_id uuid NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  user_id uuid NOT NULL REFERENCES users(id),
  role text NOT NULL CHECK (role IN ('owner', 'manager', 'member')),
  joined_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  updated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  PRIMARY KEY (project_id, user_id)
);

CREATE INDEX project_members_user_project_idx ON project_members (user_id, project_id);
CREATE INDEX project_members_project_role_idx ON project_members (project_id, role);

CREATE TABLE tasks (
  id uuid PRIMARY KEY,
  project_id uuid NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  title text NOT NULL CHECK (char_length(btrim(title)) BETWEEN 1 AND 500),
  description text NOT NULL DEFAULT '' CHECK (char_length(description) <= 50000),
  status text NOT NULL DEFAULT 'todo'
    CHECK (status IN ('todo', 'in_progress', 'done', 'cancelled')),
  priority text NOT NULL DEFAULT 'medium'
    CHECK (priority IN ('low', 'medium', 'high', 'urgent')),
  creator_id uuid NOT NULL REFERENCES users(id),
  assignee_id uuid REFERENCES users(id),
  deadline_at timestamptz,
  completed_at timestamptz,
  version bigint NOT NULL DEFAULT 1 CHECK (version > 0),
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  updated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  deleted_at timestamptz,
  deleted_by uuid REFERENCES users(id),
  FOREIGN KEY (project_id, assignee_id)
    REFERENCES project_members(project_id, user_id),
  CHECK ((status = 'done') = (completed_at IS NOT NULL)),
  CHECK ((deleted_at IS NULL) = (deleted_by IS NULL))
);

CREATE INDEX tasks_project_created_idx ON tasks (project_id, created_at DESC, id)
  WHERE deleted_at IS NULL;
CREATE INDEX tasks_project_updated_idx ON tasks (project_id, updated_at DESC, id)
  WHERE deleted_at IS NULL;
CREATE INDEX tasks_project_deadline_idx ON tasks (project_id, deadline_at, id)
  WHERE deleted_at IS NULL AND deadline_at IS NOT NULL;
CREATE INDEX tasks_project_status_idx ON tasks (project_id, status, id)
  WHERE deleted_at IS NULL;
CREATE INDEX tasks_project_priority_idx ON tasks (project_id, priority, id)
  WHERE deleted_at IS NULL;
CREATE INDEX tasks_assignee_idx ON tasks (project_id, assignee_id, id)
  WHERE deleted_at IS NULL AND assignee_id IS NOT NULL;
CREATE INDEX tasks_creator_idx ON tasks (project_id, creator_id, id)
  WHERE deleted_at IS NULL;
CREATE INDEX tasks_title_lower_idx ON tasks (project_id, lower(title), id)
  WHERE deleted_at IS NULL;

CREATE TABLE comments (
  id uuid PRIMARY KEY,
  task_id uuid NOT NULL REFERENCES tasks(id),
  author_id uuid NOT NULL REFERENCES users(id),
  body text NOT NULL CHECK (char_length(btrim(body)) BETWEEN 1 AND 10000),
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  updated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  deleted_at timestamptz,
  deleted_by uuid REFERENCES users(id),
  CHECK ((deleted_at IS NULL) = (deleted_by IS NULL))
);

CREATE INDEX comments_task_created_idx ON comments (task_id, created_at, id)
  WHERE deleted_at IS NULL;
CREATE INDEX comments_author_idx ON comments (author_id);

CREATE TABLE audit_events (
  id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  event_id uuid NOT NULL UNIQUE,
  project_id uuid NOT NULL REFERENCES projects(id),
  task_id uuid REFERENCES tasks(id),
  actor_type text NOT NULL CHECK (actor_type IN ('user', 'system')),
  actor_user_id uuid REFERENCES users(id),
  event_type text NOT NULL CHECK (event_type <> ''),
  entity_type text NOT NULL CHECK (entity_type <> ''),
  entity_id uuid NOT NULL,
  before_data jsonb,
  after_data jsonb,
  correlation_id text NOT NULL CHECK (correlation_id <> ''),
  occurred_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  CHECK ((actor_type = 'user') = (actor_user_id IS NOT NULL)),
  CHECK (before_data IS NULL OR jsonb_typeof(before_data) = 'object'),
  CHECK (after_data IS NULL OR jsonb_typeof(after_data) = 'object')
);

CREATE INDEX audit_events_project_history_idx
  ON audit_events (project_id, occurred_at DESC, id DESC);
CREATE INDEX audit_events_task_history_idx
  ON audit_events (task_id, occurred_at DESC, id DESC)
  WHERE task_id IS NOT NULL;
CREATE INDEX audit_events_actor_idx ON audit_events (actor_user_id, occurred_at DESC)
  WHERE actor_user_id IS NOT NULL;

CREATE FUNCTION reject_audit_event_mutation() RETURNS trigger
LANGUAGE plpgsql AS $$
BEGIN
  RAISE EXCEPTION 'audit events are immutable';
END;
$$;

CREATE TRIGGER audit_events_immutable
BEFORE UPDATE OR DELETE ON audit_events
FOR EACH ROW EXECUTE FUNCTION reject_audit_event_mutation();

CREATE TABLE outbox_events (
  event_id uuid PRIMARY KEY,
  project_id uuid REFERENCES projects(id),
  aggregate_type text NOT NULL CHECK (aggregate_type <> ''),
  aggregate_id uuid NOT NULL,
  event_type text NOT NULL CHECK (event_type <> ''),
  payload jsonb NOT NULL CHECK (jsonb_typeof(payload) = 'object'),
  correlation_id text NOT NULL CHECK (correlation_id <> ''),
  occurred_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  available_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  attempts integer NOT NULL DEFAULT 0 CHECK (attempts >= 0),
  locked_by text,
  locked_until timestamptz,
  processed_at timestamptz,
  last_error text,
  CHECK ((locked_by IS NULL) = (locked_until IS NULL)),
  CHECK (processed_at IS NULL OR locked_by IS NULL)
);

CREATE INDEX outbox_events_claim_idx ON outbox_events (available_at, occurred_at, event_id)
  WHERE processed_at IS NULL;
CREATE INDEX outbox_events_locked_idx ON outbox_events (locked_until)
  WHERE processed_at IS NULL AND locked_until IS NOT NULL;
CREATE INDEX outbox_events_project_idx ON outbox_events (project_id, occurred_at, event_id)
  WHERE project_id IS NOT NULL;

CREATE TABLE notification_events (
  sequence_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  event_id uuid NOT NULL,
  source_event_id uuid NOT NULL REFERENCES outbox_events(event_id),
  recipient_user_id uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  project_id uuid REFERENCES projects(id),
  event_type text NOT NULL CHECK (event_type <> ''),
  entity_id uuid,
  payload jsonb NOT NULL CHECK (jsonb_typeof(payload) = 'object'),
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  expires_at timestamptz NOT NULL,
  acknowledged_at timestamptz,
  UNIQUE (recipient_user_id, event_id),
  CHECK (expires_at > created_at),
  CHECK (acknowledged_at IS NULL OR acknowledged_at >= created_at)
);

CREATE INDEX notification_events_recipient_replay_idx
  ON notification_events (recipient_user_id, sequence_id);
CREATE INDEX notification_events_retention_idx ON notification_events (expires_at);
CREATE INDEX notification_events_project_recipient_idx
  ON notification_events (project_id, recipient_user_id, sequence_id)
  WHERE project_id IS NOT NULL;

CREATE TABLE jobs (
  id uuid PRIMARY KEY,
  business_key text NOT NULL UNIQUE CHECK (business_key <> ''),
  job_type text NOT NULL CHECK (job_type <> ''),
  payload jsonb NOT NULL CHECK (jsonb_typeof(payload) = 'object'),
  status text NOT NULL DEFAULT 'pending'
    CHECK (status IN ('pending', 'running', 'succeeded', 'failed', 'cancelled')),
  scheduled_at timestamptz NOT NULL,
  attempt integer NOT NULL DEFAULT 0 CHECK (attempt >= 0),
  max_attempts integer NOT NULL CHECK (max_attempts > 0),
  lease_owner text,
  lease_expires_at timestamptz,
  last_error text,
  correlation_id text NOT NULL CHECK (correlation_id <> ''),
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  updated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  completed_at timestamptz,
  CHECK (attempt <= max_attempts),
  CHECK ((lease_owner IS NULL) = (lease_expires_at IS NULL)),
  CHECK ((status IN ('succeeded', 'failed', 'cancelled')) = (completed_at IS NOT NULL)),
  CHECK ((status = 'running') = (lease_owner IS NOT NULL))
);

CREATE INDEX jobs_due_idx ON jobs (scheduled_at, id)
  WHERE status = 'pending';
CREATE INDEX jobs_lease_recovery_idx ON jobs (lease_expires_at, id)
  WHERE status = 'running';
CREATE INDEX jobs_type_status_idx ON jobs (job_type, status, scheduled_at);
