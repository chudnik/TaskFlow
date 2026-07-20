#include "taskflow/infrastructure/jwt_access_token.hpp"

#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>

namespace {
using namespace taskflow;

class Validator final : public application::AccountSessionValidator {
public:
  bool active{true};
  bool account_and_session_active(const domain::Uuid &, const domain::Uuid &) const override {
    return active;
  }
};

TEST(JwtAccessToken, CreatesAndStrictlyValidatesClaimsAndCurrentState) {
  domain::FixedClock clock{*domain::parse_utc("2026-07-20T10:00:00Z")};
  Validator validator;
  infrastructure::JwtAccessTokenService tokens{
      "test-signing-secret-with-at-least-32-bytes", "taskflow", "taskflow-api",
      std::chrono::minutes{15}, clock, validator};
  const domain::User user{domain::Uuid::generate(), "user@example.com", domain::GlobalRole::user,
                          domain::AccountStatus::active, clock.now(), clock.now()};
  const auto session_id = domain::Uuid::generate();
  const auto encoded = tokens.create(user, session_id);
  const auto principal = tokens.validate(encoded);
  ASSERT_TRUE(principal);
  EXPECT_EQ(principal->user_id, user.id);
  EXPECT_EQ(principal->session_id, session_id);

  validator.active = false;
  EXPECT_FALSE(tokens.validate(encoded));
}

TEST(JwtAccessToken, RejectsWrongIssuerAudienceSignatureAndExpiry) {
  domain::FixedClock clock{*domain::parse_utc("2026-07-20T10:00:00Z")};
  Validator validator;
  infrastructure::JwtAccessTokenService tokens{
      "test-signing-secret-with-at-least-32-bytes", "taskflow", "taskflow-api",
      std::chrono::seconds{30}, clock, validator};
  const domain::User user{domain::Uuid::generate(), "user@example.com", domain::GlobalRole::user,
                          domain::AccountStatus::active, clock.now(), clock.now()};
  const auto encoded = tokens.create(user, domain::Uuid::generate());
  infrastructure::JwtAccessTokenService wrong_issuer{
      "test-signing-secret-with-at-least-32-bytes", "other", "taskflow-api",
      std::chrono::seconds{30}, clock, validator};
  EXPECT_FALSE(wrong_issuer.validate(encoded));
  infrastructure::JwtAccessTokenService wrong_secret{
      "different-signing-secret-at-least-32-bytes", "taskflow", "taskflow-api",
      std::chrono::seconds{30}, clock, validator};
  EXPECT_FALSE(wrong_secret.validate(encoded));
  clock.advance(std::chrono::seconds{31});
  EXPECT_FALSE(tokens.validate(encoded));
}
} // namespace
