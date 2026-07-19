#include "taskflow/infrastructure/postgres.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

namespace taskflow::infrastructure {
namespace {

[[nodiscard]] std::string integration_dsn() {
  const char *value = std::getenv("TASKFLOW_TEST_POSTGRES_DSN");
  return value == nullptr ? std::string{} : std::string{value};
}

TEST(PostgresFoundationIntegrationTest, ParameterizesQueriesAndPreservesNulls) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  PostgresConnection connection{dsn};
  const auto result = connection.execute("SELECT $1::text AS value, $2::text AS empty_value",
                                         {std::string{"safe-value"}, std::nullopt});
  ASSERT_EQ(result.row_count(), 1U);
  EXPECT_EQ(result.column_name(0), "value");
  EXPECT_EQ(result.value(0, 0), QueryParameter{"safe-value"});
  EXPECT_EQ(result.value(0, 1), std::nullopt);
  EXPECT_TRUE(connection.is_healthy());
}

TEST(PostgresFoundationIntegrationTest, RollsBackUncommittedTransaction) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  PostgresConnection connection{dsn};
  {
    auto transaction = connection.transaction();
    static_cast<void>(transaction.execute("CREATE TEMP TABLE rollback_probe(value integer)"));
    static_cast<void>(transaction.execute("INSERT INTO rollback_probe VALUES (1)"));
  }
  EXPECT_THROW(static_cast<void>(connection.execute("SELECT * FROM rollback_probe")),
               RepositoryError);
}

TEST(PostgresFoundationIntegrationTest, MapsConstraintErrorsBySqlState) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  PostgresConnection connection{dsn};
  try {
    static_cast<void>(connection.execute("SELECT 1 / 0"));
    FAIL() << "query should fail";
  } catch (const RepositoryError &error) {
    EXPECT_EQ(error.code(), RepositoryErrorCode::unexpected);
    EXPECT_EQ(error.sql_state(), "22012");
    EXPECT_STREQ(error.what(), "PostgreSQL query failed");
  }
}

TEST(PostgresFoundationIntegrationTest, ResetRequiresExplicitSessionOptIn) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  PostgresConnection connection{dsn};
  EXPECT_THROW(reset_database_for_tests(connection), RepositoryError);
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  EXPECT_NO_THROW(reset_database_for_tests(connection));
  const auto count = connection.execute("SELECT count(*) FROM users");
  ASSERT_EQ(count.row_count(), 1U);
  EXPECT_EQ(count.value(0, 0), QueryParameter{"0"});
}

} // namespace
} // namespace taskflow::infrastructure
