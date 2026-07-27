#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace taskflow::infrastructure {

enum class RepositoryErrorCode {
  not_found,
  conflict,
  constraint_violation,
  serialization_failure,
  timeout,
  unavailable,
  unexpected,
};

class RepositoryError : public std::runtime_error {
public:
  RepositoryError(RepositoryErrorCode code, std::string message, std::string sql_state = {});

  [[nodiscard]] RepositoryErrorCode code() const noexcept;
  [[nodiscard]] const std::string &sql_state() const noexcept;

private:
  RepositoryErrorCode code_;
  std::string sql_state_;
};

using QueryParameter = std::optional<std::string>;
using QueryParameters = std::vector<QueryParameter>;

class QueryResult {
public:
  [[nodiscard]] std::size_t row_count() const noexcept;
  [[nodiscard]] std::size_t column_count() const noexcept;
  [[nodiscard]] std::size_t affected_rows() const noexcept;
  [[nodiscard]] const std::string &column_name(std::size_t column) const;
  [[nodiscard]] const QueryParameter &value(std::size_t row, std::size_t column) const;

private:
  friend class PostgresConnection;
  std::vector<std::string> columns_;
  std::vector<std::vector<QueryParameter>> rows_;
  std::size_t affected_rows_{0};
};

class Transaction;

class PostgresConnection {
public:
  explicit PostgresConnection(std::string dsn);
  ~PostgresConnection();

  PostgresConnection(const PostgresConnection &) = delete;
  PostgresConnection &operator=(const PostgresConnection &) = delete;
  PostgresConnection(PostgresConnection &&) noexcept;
  PostgresConnection &operator=(PostgresConnection &&) noexcept;

  [[nodiscard]] QueryResult execute(std::string_view sql, const QueryParameters &parameters = {});
  [[nodiscard]] bool is_healthy() noexcept;
  [[nodiscard]] Transaction transaction();

private:
  friend class Transaction;
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool transaction_active_{false};
};

class Transaction {
public:
  ~Transaction();

  Transaction(const Transaction &) = delete;
  Transaction &operator=(const Transaction &) = delete;
  Transaction(Transaction &&other) noexcept;
  Transaction &operator=(Transaction &&) = delete;

  [[nodiscard]] QueryResult execute(std::string_view sql, const QueryParameters &parameters = {});
  void commit();
  void rollback();

private:
  friend class PostgresConnection;
  explicit Transaction(PostgresConnection &connection);
  PostgresConnection *connection_;
  bool active_{true};
};

void reset_database_for_tests(PostgresConnection &connection);

} // namespace taskflow::infrastructure
