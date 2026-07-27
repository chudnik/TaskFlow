CREATE TABLE reminder_effects (
  effect_key text PRIMARY KEY CHECK (effect_key <> ''),
  event_id uuid NOT NULL UNIQUE,
  created_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
