#include "taskflow/infrastructure/schema_compatibility.hpp"

#include <sstream>
#include <stdexcept>

#if TASKFLOW_HAS_POSTGRES
#include <libpq-fe.h>
#endif

namespace taskflow::infrastructure {

bool SchemaCompatibilityResult::is_compatible() const noexcept {
  return status == SchemaCompatibility::compatible;
}

std::string SchemaCompatibilityResult::message() const {
  std::ostringstream output;
  switch (status) {
  case SchemaCompatibility::compatible:
    output << "database schema version " << current_version << " is compatible";
    break;
  case SchemaCompatibility::metadata_missing:
    output << "database migration metadata is missing; run taskflow-migrate";
    break;
  case SchemaCompatibility::migration_in_progress:
    output << "database migration is currently in progress";
    break;
  case SchemaCompatibility::version_too_old:
    output << "database schema version " << current_version << " is older than required version "
           << expected_schema_version << "; run taskflow-migrate";
    break;
  case SchemaCompatibility::version_too_new:
    output << "database schema version " << current_version << " is newer than supported version "
           << expected_schema_version << "; deploy a compatible application build";
    break;
  }
  return output.str();
}

SchemaCompatibilityResult evaluate_schema_compatibility(const bool metadata_exists,
                                                        const bool migration_in_progress,
                                                        const std::int64_t current_version,
                                                        const std::int64_t required_version) {
  if (!metadata_exists) {
    return {SchemaCompatibility::metadata_missing, current_version};
  }
  if (migration_in_progress) {
    return {SchemaCompatibility::migration_in_progress, current_version};
  }
  if (current_version < required_version) {
    return {SchemaCompatibility::version_too_old, current_version};
  }
  if (current_version > required_version) {
    return {SchemaCompatibility::version_too_new, current_version};
  }
  return {SchemaCompatibility::compatible, current_version};
}

SchemaCompatibilityResult check_postgres_schema(const std::string &dsn) {
#if TASKFLOW_HAS_POSTGRES
  PGconn *connection = PQconnectdb(dsn.c_str());
  if (connection == nullptr) {
    throw std::runtime_error{"unable to allocate PostgreSQL connection"};
  }
  struct ConnectionGuard {
    PGconn *value;
    ~ConnectionGuard() { PQfinish(value); }
  } guard{connection};

  if (PQstatus(connection) != CONNECTION_OK) {
    throw std::runtime_error{"unable to connect to PostgreSQL for schema compatibility check"};
  }

  PGresult *query =
      PQexec(connection, "SELECT to_regclass('public.taskflow_schema_migrations') IS NOT NULL, "
                         "pg_try_advisory_lock(1413563980)");
  if (query == nullptr) {
    throw std::runtime_error{"schema compatibility query failed"};
  }
  struct ResultGuard {
    PGresult *value;
    ~ResultGuard() { PQclear(value); }
  } result_guard{query};

  if (PQresultStatus(query) != PGRES_TUPLES_OK || PQntuples(query) != 1) {
    throw std::runtime_error{"schema compatibility query failed"};
  }

  const bool metadata_exists = std::string{PQgetvalue(query, 0, 0)} == "t";
  const bool acquired_lock = std::string{PQgetvalue(query, 0, 1)} == "t";
  std::int64_t current_version = 0;
  if (metadata_exists) {
    PGresult *version_query =
        PQexec(connection, "SELECT COALESCE(max(version), 0) FROM taskflow_schema_migrations");
    if (version_query == nullptr || PQresultStatus(version_query) != PGRES_TUPLES_OK ||
        PQntuples(version_query) != 1) {
      if (version_query != nullptr) {
        PQclear(version_query);
      }
      throw std::runtime_error{"schema version query failed"};
    }
    current_version = std::stoll(PQgetvalue(version_query, 0, 0));
    PQclear(version_query);
  }
  if (acquired_lock) {
    PGresult *unlock = PQexec(connection, "SELECT pg_advisory_unlock(1413563980)");
    if (unlock != nullptr) {
      PQclear(unlock);
    }
  }
  return evaluate_schema_compatibility(metadata_exists, !acquired_lock, current_version);
#else
  (void)dsn;
  throw std::runtime_error{"PostgreSQL support is disabled in this build"};
#endif
}

} // namespace taskflow::infrastructure
