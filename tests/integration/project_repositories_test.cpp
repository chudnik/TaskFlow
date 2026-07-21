#include "taskflow/infrastructure/project_repositories.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

namespace {
using namespace taskflow;

std::string integration_dsn() {
  const char *value = std::getenv("TASKFLOW_TEST_POSTGRES_DSN");
  return value == nullptr ? std::string{} : std::string{value};
}

domain::Uuid insert_user(infrastructure::PostgresConnection &connection, const std::string &email) {
  const auto id = domain::Uuid::generate();
  static_cast<void>(connection.execute(
      "INSERT INTO users(id, email, password_hash) VALUES($1::uuid, $2, 'test-hash')",
      {id.to_string(), email}));
  return id;
}

TEST(ProjectRepositoriesIntegration, CreatesProjectAndOwnerAtomically) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto owner = insert_user(connection, "project-owner@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::ProjectMembershipRepository memberships{connection};

  const auto project = projects.create_with_owner("TaskFlow", "Project repository test", owner);
  const auto membership = memberships.find(project.id, owner);
  ASSERT_TRUE(membership);
  EXPECT_EQ(membership->role, domain::ProjectRole::owner);
  EXPECT_EQ(project.created_by, owner);
}

TEST(ProjectRepositoriesIntegration, RefusesToRemoveOrDemoteFinalOwner) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto first_owner = insert_user(connection, "first-owner@example.com");
  const auto second_owner = insert_user(connection, "second-owner@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::ProjectMembershipRepository memberships{connection};
  const auto project = projects.create_with_owner("Invariant", "", first_owner);

  EXPECT_THROW(memberships.remove(project.id, first_owner), infrastructure::RepositoryError);
  EXPECT_THROW(static_cast<void>(
                   memberships.change_role(project.id, first_owner, domain::ProjectRole::manager)),
               infrastructure::RepositoryError);

  static_cast<void>(memberships.add(project.id, second_owner, domain::ProjectRole::owner));
  EXPECT_NO_THROW(memberships.remove(project.id, first_owner));
  const auto remaining = memberships.list(project.id);
  ASSERT_EQ(remaining.size(), 1U);
  EXPECT_EQ(remaining.front().user_id, second_owner);
  EXPECT_EQ(remaining.front().role, domain::ProjectRole::owner);
}

} // namespace
