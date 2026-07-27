#include "taskflow/application/authentication_middleware.hpp"
#include "taskflow/application/authentication_sessions.hpp"
#include "taskflow/transport/http/identity_controller.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>

namespace {

using namespace taskflow;

class FixedClock final : public domain::Clock {
public:
  domain::UtcInstant now() const override { return *domain::parse_utc("2026-07-28T10:00:00Z"); }
};

class Passwords final : public application::PasswordService {
public:
  std::string hash_password(const std::string_view password) const override {
    return "hash:" + std::string{password};
  }
  bool verify_password(const std::string_view password,
                       const std::string_view hash) const noexcept override {
    return hash == "hash:" + std::string{password};
  }
};

class Identities final : public application::IdentityStore {
public:
  domain::User user{domain::Uuid::generate(),
                    "user@example.com",
                    domain::GlobalRole::user,
                    domain::AccountStatus::active,
                    *domain::parse_utc("2026-07-28T09:00:00Z"),
                    *domain::parse_utc("2026-07-28T09:00:00Z")};

  domain::User create_user(std::string, std::string) override { return user; }
  std::optional<application::StoredCredential> find_credentials(std::string_view) override {
    return application::StoredCredential{user, "hash:correct password"};
  }
};

class Tokens final : public application::AccessTokenService {
public:
  std::optional<application::AuthenticatedPrincipal> principal;

  std::string create(const domain::User &, const domain::Uuid &session_id) const override {
    return "access:" + session_id.to_string();
  }
  std::optional<application::AuthenticatedPrincipal>
  validate(std::string_view) const noexcept override {
    return principal;
  }
};

class Sessions final : public application::AuthenticationSessionStore {
public:
  domain::Uuid session_id{domain::Uuid::generate()};
  application::SessionRotation rotation{application::SessionRotationStatus::invalid, std::nullopt};
  domain::UtcInstant created_expiry{};
  domain::UtcInstant rotated_expiry{};
  std::optional<domain::Uuid> logged_out;

  application::IssuedSession create(const domain::User &user,
                                    const domain::UtcInstant expires_at) override {
    created_expiry = expires_at;
    return {user, session_id, "refresh-created", expires_at};
  }
  application::SessionRotation rotate(std::string_view,
                                      const domain::UtcInstant expires_at) override {
    rotated_expiry = expires_at;
    return rotation;
  }
  void logout(const domain::Uuid &id) override { logged_out = id; }
};

struct Fixture {
  FixedClock clock;
  Passwords passwords;
  Identities identities;
  application::IdentityUseCases identity{identities, passwords};
  Sessions sessions;
  Tokens tokens;
  application::AuthenticationSessionUseCases use_cases{identity, sessions, tokens, clock};
};

TEST(AuthenticationSessionsTest, RegistrationAndLoginIssueConfiguredTokenLifetimes) {
  Fixture fixture;

  const auto registered = fixture.use_cases.register_user("user@example.com", "correct password");
  EXPECT_EQ(registered.refresh_token, "refresh-created");
  EXPECT_EQ(registered.access_token, "access:" + fixture.sessions.session_id.to_string());
  EXPECT_EQ(registered.access_expires_at, fixture.clock.now() + application::access_token_lifetime);
  EXPECT_EQ(registered.refresh_expires_at,
            fixture.clock.now() + application::refresh_session_lifetime);

  const auto logged_in = fixture.use_cases.login("user@example.com", "correct password");
  EXPECT_EQ(logged_in.user.id, fixture.identities.user.id);
  EXPECT_EQ(fixture.sessions.created_expiry,
            fixture.clock.now() + application::refresh_session_lifetime);
}

TEST(AuthenticationSessionsTest, RefreshRotatesAndIssuesNewAccessToken) {
  Fixture fixture;
  fixture.sessions.rotation = {
      application::SessionRotationStatus::rotated,
      application::IssuedSession{fixture.identities.user, fixture.sessions.session_id,
                                 "refresh-rotated",
                                 fixture.clock.now() + application::refresh_session_lifetime}};

  const auto result = fixture.use_cases.refresh("refresh-presented");

  EXPECT_EQ(result.refresh_token, "refresh-rotated");
  EXPECT_EQ(result.access_token, "access:" + fixture.sessions.session_id.to_string());
  EXPECT_EQ(fixture.sessions.rotated_expiry,
            fixture.clock.now() + application::refresh_session_lifetime);
}

TEST(AuthenticationSessionsTest, InvalidAndReplayedRefreshTokensHaveSanitizedErrors) {
  Fixture fixture;
  for (const auto status : {application::SessionRotationStatus::invalid,
                            application::SessionRotationStatus::replay_detected}) {
    fixture.sessions.rotation = {status, std::nullopt};
    try {
      static_cast<void>(fixture.use_cases.refresh("secret-refresh-token"));
      FAIL() << "expected AuthenticationSessionError";
    } catch (const application::AuthenticationSessionError &error) {
      EXPECT_EQ(std::string{error.what()}.find("secret-refresh-token"), std::string::npos);
    }
  }
}

TEST(AuthenticationSessionsTest, LogoutRevokesCurrentSession) {
  Fixture fixture;
  fixture.use_cases.logout(fixture.sessions.session_id);
  ASSERT_TRUE(fixture.sessions.logged_out.has_value());
  EXPECT_EQ(*fixture.sessions.logged_out, fixture.sessions.session_id);
}

TEST(AuthenticationSessionsTest, HttpControllerSerializesTokensAndAuthenticatesLogout) {
  Fixture fixture;
  application::AuthenticationMiddleware authentication{fixture.tokens};
  transport::http::IdentityController controller{fixture.use_cases, authentication};

  const auto registration =
      controller.register_user(R"({"email":"user@example.com","password":"correct password"})");
  EXPECT_EQ(registration.status, 201);
  EXPECT_NE(registration.body.find("\"access_token\""), std::string::npos);
  EXPECT_NE(registration.body.find("\"refresh_token\""), std::string::npos);
  EXPECT_NE(registration.body.find("\"access_expires_at\""), std::string::npos);

  EXPECT_EQ(controller.refresh(R"({"refresh_token":"invalid"})").status, 401);
  EXPECT_EQ(controller.logout("Bearer invalid").status, 401);

  fixture.tokens.principal = application::AuthenticatedPrincipal{
      fixture.identities.user.id, fixture.sessions.session_id, domain::GlobalRole::user,
      fixture.clock.now() + application::access_token_lifetime};
  EXPECT_EQ(controller.logout("Bearer valid").status, 204);
  EXPECT_EQ(fixture.sessions.logged_out, fixture.sessions.session_id);
}

} // namespace
