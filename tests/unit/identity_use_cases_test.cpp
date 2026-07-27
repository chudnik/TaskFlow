#include "taskflow/application/identity.hpp"
#include "taskflow/transport/http/identity_controller.hpp"

#include <gtest/gtest.h>

#include <optional>

namespace {
using namespace taskflow;

domain::User test_user(domain::AccountStatus status = domain::AccountStatus::active) {
  const auto now = *domain::parse_utc("2026-07-20T10:00:00Z");
  return {domain::Uuid::generate(), "user@example.com", domain::GlobalRole::user, status, now, now};
}

class FakePasswords final : public application::PasswordService {
public:
  std::string hash_password(std::string_view password) const override {
    return "hash:" + std::string{password};
  }
  bool verify_password(std::string_view password, std::string_view hash) const noexcept override {
    return hash == "hash:" + std::string{password};
  }
};

class FakeStore final : public application::IdentityStore {
public:
  bool duplicate{false};
  std::optional<application::StoredCredential> credential;

  domain::User create_user(std::string, std::string) override {
    if (duplicate) {
      throw application::IdentityError{application::IdentityErrorCode::duplicate_email,
                                       "email is already registered"};
    }
    return test_user();
  }
  std::optional<application::StoredCredential> find_credentials(std::string_view) override {
    return credential;
  }
};

TEST(IdentityUseCases, ControllerMapsDuplicateEmailToConflict) {
  FakeStore store;
  store.duplicate = true;
  FakePasswords passwords;
  const application::IdentityUseCases use_cases{store, passwords};
  const transport::http::IdentityController controller{use_cases};
  EXPECT_EQ(
      controller.register_user(R"({"email":"user@example.com","password":"long password value"})")
          .status,
      409);
}

TEST(IdentityUseCases, LoginDoesNotDistinguishMissingUserAndWrongPassword) {
  FakeStore store;
  FakePasswords passwords;
  const application::IdentityUseCases use_cases{store, passwords};
  EXPECT_EQ(transport::http::IdentityController{use_cases}
                .login(R"({"email":"missing@example.com","password":"wrong password"})")
                .status,
            401);
  store.credential = application::StoredCredential{test_user(), "hash:actual password"};
  EXPECT_EQ(transport::http::IdentityController{use_cases}
                .login(R"({"email":"user@example.com","password":"wrong password"})")
                .status,
            401);
}

TEST(IdentityUseCases, LoginRejectsInactiveAccount) {
  FakeStore store;
  store.credential = application::StoredCredential{test_user(domain::AccountStatus::inactive),
                                                   "hash:actual password"};
  FakePasswords passwords;
  const application::IdentityUseCases use_cases{store, passwords};
  EXPECT_EQ(transport::http::IdentityController{use_cases}
                .login(R"({"email":"user@example.com","password":"actual password"})")
                .status,
            403);
}
} // namespace
