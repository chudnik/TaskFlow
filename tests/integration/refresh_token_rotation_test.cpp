#include "taskflow/infrastructure/authentication_sessions.hpp"
#include "taskflow/infrastructure/refresh_tokens.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

namespace {
using namespace taskflow;

std::string integration_dsn() {
  const char *value = std::getenv("TASKFLOW_TEST_POSTGRES_DSN");
  return value == nullptr ? std::string{} : std::string{value};
}

domain::Uuid insert_user(infrastructure::PostgresConnection &connection) {
  const auto id = domain::Uuid::generate();
  static_cast<void>(
      connection.execute("INSERT INTO users(id, email, password_hash) VALUES($1::uuid, $2, $3)",
                         {id.to_string(), "refresh@example.com", "not-a-public-credential"}));
  return id;
}

TEST(RefreshTokenIntegration, RotatesOnceAndReplayRevokesTokenFamily) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto user_id = insert_user(connection);
  infrastructure::RefreshTokenService tokens{connection};
  const auto issued =
      tokens.create_session(user_id, domain::SystemClock{}.now() + std::chrono::hours{24});
  const auto rotated = tokens.rotate(issued.token);
  ASSERT_EQ(rotated.status, infrastructure::RefreshRotationStatus::rotated);
  ASSERT_TRUE(rotated.issued);
  EXPECT_NE(rotated.issued->token, issued.token);

  const auto replay = tokens.rotate(issued.token);
  EXPECT_EQ(replay.status, infrastructure::RefreshRotationStatus::replay_detected);
  const auto session = connection.execute(
      "SELECT revoked_at IS NOT NULL, revoke_reason FROM sessions WHERE id = $1::uuid",
      {issued.session_id.to_string()});
  EXPECT_EQ(session.value(0, 0), infrastructure::QueryParameter{"t"});
  EXPECT_EQ(session.value(0, 1), infrastructure::QueryParameter{"refresh_token_replay"});
  EXPECT_EQ(tokens.rotate(rotated.issued->token).status,
            infrastructure::RefreshRotationStatus::replay_detected);
}

TEST(RefreshTokenIntegration, LogoutRevokesSessionToken) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  infrastructure::RefreshTokenService tokens{connection};
  const auto issued = tokens.create_session(insert_user(connection),
                                            domain::SystemClock{}.now() + std::chrono::hours{24});
  tokens.logout(issued.session_id);
  EXPECT_EQ(tokens.rotate(issued.token).status,
            infrastructure::RefreshRotationStatus::replay_detected);
}

TEST(RefreshTokenIntegration, ApplicationAdapterRotatesAndValidatorTracksSessionState) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto user_id = insert_user(connection);
  infrastructure::UserRepository users{connection};
  infrastructure::SessionRepository sessions{connection};
  infrastructure::RefreshTokenService refresh_tokens{connection};
  infrastructure::PostgresAuthenticationSessionStore store{refresh_tokens, sessions, users};
  infrastructure::PostgresAccountSessionValidator validator{connection};
  const auto user = users.find_by_id(user_id);
  ASSERT_TRUE(user);
  const auto initial_expiry = domain::SystemClock{}.now() + std::chrono::hours{24};

  const auto issued = store.create(*user, initial_expiry);
  EXPECT_TRUE(validator.account_and_session_active(user_id, issued.session_id));

  const auto renewed_expiry = domain::SystemClock{}.now() + std::chrono::hours{24 * 30};
  const auto rotated = store.rotate(issued.refresh_token, renewed_expiry);
  ASSERT_EQ(rotated.status, application::SessionRotationStatus::rotated);
  ASSERT_TRUE(rotated.session);
  EXPECT_NE(rotated.session->refresh_token, issued.refresh_token);
  EXPECT_EQ(rotated.session->refresh_expires_at, renewed_expiry);

  store.logout(issued.session_id);
  EXPECT_FALSE(validator.account_and_session_active(user_id, issued.session_id));
  EXPECT_EQ(store.rotate(rotated.session->refresh_token, renewed_expiry).status,
            application::SessionRotationStatus::replay_detected);
}
} // namespace
