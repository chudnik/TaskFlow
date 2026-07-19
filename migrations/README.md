# Database migrations

Place versioned SQL migrations in this directory. The migration container applies
all `*.sql` files in lexical order and stops on the first PostgreSQL error.
