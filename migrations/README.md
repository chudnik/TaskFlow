# Database migrations

Place forward-only SQL migrations in this directory using names such as
`V0001__initial_schema.sql`. Versions must start at 1 and remain contiguous.

The explicit `migrations` container serializes runners with a PostgreSQL advisory lock, applies
each pending file in its own transaction, and records its version and SHA-256 checksum in
`taskflow_schema_migrations`. It refuses gaps, malformed names, and edits to already-applied
migrations. API and worker startup require the recorded version to exactly match
`expected_schema_version` in `schema_compatibility.hpp`; update that constant in the same change
that adds a migration.
