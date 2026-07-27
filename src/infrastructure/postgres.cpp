#include "taskflow/infrastructure/postgres.hpp"

#include <charconv>
#include <limits>
#include <utility>

#if TASKFLOW_HAS_POSTGRES
#include <libpq-fe.h>
#endif

namespace taskflow::infrastructure {
namespace {

#if TASKFLOW_HAS_POSTGRES
[[nodiscard]] RepositoryErrorCode classify_sql_state(const std::string_view state) {
  if (state == "23505") {
    return RepositoryErrorCode::conflict;
  }
  if (state == "23503" || state == "23514" || state == "23502" || state == "22P02") {
    return RepositoryErrorCode::constraint_violation;
  }
  if (state == "40001" || state == "40P01") {
    return RepositoryErrorCode::serialization_failure;
  }
  if (state == "57014") {
    return RepositoryErrorCode::timeout;
  }
  if (state.starts_with("08")) {
    return RepositoryErrorCode::unavailable;
  }
  return RepositoryErrorCode::unexpected;
}

[[nodiscard]] std::size_t parse_affected_rows(const char *value) noexcept {
  if (value == nullptr || *value == '\0') {
    return 0;
  }
  std::size_t result = 0;
  const std::string_view text{value};
  const auto conversion = std::from_chars(text.data(), text.data() + text.size(), result);
  return conversion.ec == std::errc{} ? result : 0;
}
#endif

} // namespace

RepositoryError::RepositoryError(const RepositoryErrorCode code, std::string message,
                                 std::string sql_state)
    : std::runtime_error{std::move(message)}, code_{code}, sql_state_{std::move(sql_state)} {}

RepositoryErrorCode RepositoryError::code() const noexcept { return code_; }

const std::string &RepositoryError::sql_state() const noexcept { return sql_state_; }

std::size_t QueryResult::row_count() const noexcept { return rows_.size(); }

std::size_t QueryResult::column_count() const noexcept { return columns_.size(); }

std::size_t QueryResult::affected_rows() const noexcept { return affected_rows_; }

const std::string &QueryResult::column_name(const std::size_t column) const {
  return columns_.at(column);
}

const QueryParameter &QueryResult::value(const std::size_t row, const std::size_t column) const {
  return rows_.at(row).at(column);
}

struct PostgresConnection::Impl {
#if TASKFLOW_HAS_POSTGRES
  PGconn *connection{nullptr};
#endif
};

PostgresConnection::PostgresConnection(std::string dsn) : impl_{std::make_unique<Impl>()} {
#if TASKFLOW_HAS_POSTGRES
  impl_->connection = PQconnectdb(dsn.c_str());
  if (impl_->connection == nullptr || PQstatus(impl_->connection) != CONNECTION_OK) {
    if (impl_->connection != nullptr) {
      PQfinish(impl_->connection);
      impl_->connection = nullptr;
    }
    throw RepositoryError{RepositoryErrorCode::unavailable, "unable to connect to PostgreSQL"};
  }
  PGresult *timeout_result = PQexec(impl_->connection, "SET statement_timeout = '5s'");
  if (timeout_result != nullptr) {
    PQclear(timeout_result);
  }
#else
  (void)dsn;
  throw RepositoryError{RepositoryErrorCode::unavailable,
                        "PostgreSQL support is disabled in this build"};
#endif
}

PostgresConnection::~PostgresConnection() {
#if TASKFLOW_HAS_POSTGRES
  if (impl_ && impl_->connection != nullptr) {
    PQfinish(impl_->connection);
  }
#endif
}

PostgresConnection::PostgresConnection(PostgresConnection &&) noexcept = default;
PostgresConnection &PostgresConnection::operator=(PostgresConnection &&) noexcept = default;

QueryResult PostgresConnection::execute(const std::string_view sql,
                                        const QueryParameters &parameters) {
#if TASKFLOW_HAS_POSTGRES
  if (!impl_ || impl_->connection == nullptr || PQstatus(impl_->connection) != CONNECTION_OK) {
    throw RepositoryError{RepositoryErrorCode::unavailable, "PostgreSQL connection is unavailable",
                          "08006"};
  }
  if (parameters.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "too many query parameters"};
  }

  std::vector<const char *> values;
  values.reserve(parameters.size());
  for (const auto &parameter : parameters) {
    values.push_back(parameter ? parameter->c_str() : nullptr);
  }

  const std::string statement{sql};
  PGresult *raw_result =
      PQexecParams(impl_->connection, statement.c_str(), static_cast<int>(values.size()), nullptr,
                   values.data(), nullptr, nullptr, 0);
  if (raw_result == nullptr) {
    throw RepositoryError{RepositoryErrorCode::unavailable, "PostgreSQL query returned no result",
                          "08006"};
  }
  struct ResultGuard {
    PGresult *value;
    ~ResultGuard() { PQclear(value); }
  } guard{raw_result};

  const auto status = PQresultStatus(raw_result);
  if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
    const char *state_value = PQresultErrorField(raw_result, PG_DIAG_SQLSTATE);
    const std::string state = state_value == nullptr ? std::string{} : std::string{state_value};
    throw RepositoryError{classify_sql_state(state), "PostgreSQL query failed", state};
  }

  QueryResult result;
  result.affected_rows_ = parse_affected_rows(PQcmdTuples(raw_result));
  const int columns = PQnfields(raw_result);
  const int rows = PQntuples(raw_result);
  result.columns_.reserve(static_cast<std::size_t>(columns));
  for (int column = 0; column < columns; ++column) {
    result.columns_.emplace_back(PQfname(raw_result, column));
  }
  result.rows_.reserve(static_cast<std::size_t>(rows));
  for (int row = 0; row < rows; ++row) {
    std::vector<QueryParameter> values_row;
    values_row.reserve(static_cast<std::size_t>(columns));
    for (int column = 0; column < columns; ++column) {
      if (PQgetisnull(raw_result, row, column) != 0) {
        values_row.emplace_back(std::nullopt);
      } else {
        values_row.emplace_back(std::string{PQgetvalue(raw_result, row, column)});
      }
    }
    result.rows_.push_back(std::move(values_row));
  }
  return result;
#else
  (void)sql;
  (void)parameters;
  throw RepositoryError{RepositoryErrorCode::unavailable,
                        "PostgreSQL support is disabled in this build"};
#endif
}

bool PostgresConnection::is_healthy() noexcept {
  try {
    const auto result = execute("SELECT 1");
    return result.row_count() == 1;
  } catch (const RepositoryError &) {
    return false;
  }
}

Transaction PostgresConnection::transaction() {
  if (transaction_active_) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "nested transactions are not supported"};
  }
  return Transaction{*this};
}

Transaction::Transaction(PostgresConnection &connection) : connection_{&connection} {
  static_cast<void>(connection_->execute("BEGIN"));
  connection_->transaction_active_ = true;
}

Transaction::~Transaction() {
  if (active_ && connection_ != nullptr) {
    try {
      rollback();
    } catch (const RepositoryError &) {
      connection_->transaction_active_ = false;
    }
  }
}

Transaction::Transaction(Transaction &&other) noexcept
    : connection_{std::exchange(other.connection_, nullptr)},
      active_{std::exchange(other.active_, false)} {}

QueryResult Transaction::execute(const std::string_view sql, const QueryParameters &parameters) {
  if (!active_ || connection_ == nullptr) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "transaction is no longer active"};
  }
  return connection_->execute(sql, parameters);
}

void Transaction::commit() {
  if (!active_ || connection_ == nullptr) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "transaction is no longer active"};
  }
  static_cast<void>(connection_->execute("COMMIT"));
  connection_->transaction_active_ = false;
  active_ = false;
}

void Transaction::rollback() {
  if (!active_ || connection_ == nullptr) {
    return;
  }
  static_cast<void>(connection_->execute("ROLLBACK"));
  connection_->transaction_active_ = false;
  active_ = false;
}

void reset_database_for_tests(PostgresConnection &connection) {
  const auto opt_in = connection.execute("SELECT current_setting('taskflow.test_database', true)");
  if (opt_in.row_count() != 1 || !opt_in.value(0, 0) || *opt_in.value(0, 0) != "on") {
    throw RepositoryError{RepositoryErrorCode::unexpected,
                          "database reset requires taskflow.test_database=on"};
  }
  static_cast<void>(connection.execute(
      "TRUNCATE TABLE notification_events, jobs, outbox_events, audit_events, comments, tasks, "
      "project_members, projects, sessions, users RESTART IDENTITY CASCADE"));
}

} // namespace taskflow::infrastructure
