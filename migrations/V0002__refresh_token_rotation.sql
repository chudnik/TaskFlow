CREATE TABLE session_refresh_tokens (
  token_hash text PRIMARY KEY CHECK (length(token_hash) = 64),
  session_id uuid NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
  token_family_id uuid NOT NULL,
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  used_at timestamptz,
  revoked_at timestamptz
);

CREATE INDEX session_refresh_tokens_family_idx
  ON session_refresh_tokens(token_family_id, created_at);
CREATE INDEX session_refresh_tokens_active_session_idx
  ON session_refresh_tokens(session_id) WHERE used_at IS NULL AND revoked_at IS NULL;

INSERT INTO session_refresh_tokens(token_hash, session_id, token_family_id, created_at,
                                   revoked_at)
SELECT refresh_token_hash, id, token_family_id, created_at, revoked_at
FROM sessions;
