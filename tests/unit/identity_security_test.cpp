#include "taskflow/domain/identity.hpp"
#include "taskflow/infrastructure/password_hasher.hpp"

#include <gtest/gtest.h>

TEST(IdentitySecurity, NormalizesEmailCaseAndSurroundingWhitespace) {
  EXPECT_EQ(taskflow::domain::normalize_email("  Alice.Example@EXAMPLE.COM\n"),
            "alice.example@example.com");
  EXPECT_THROW(static_cast<void>(taskflow::domain::normalize_email("not-an-email")),
               taskflow::domain::CredentialValidationError);
}

TEST(IdentitySecurity, RequiresLongPasswordAndRejectsEmbeddedNull) {
  EXPECT_THROW(taskflow::domain::validate_password("short"),
               taskflow::domain::CredentialValidationError);
  const std::string embedded_null{"long-enough\0password", 20};
  EXPECT_THROW(taskflow::domain::validate_password(embedded_null),
               taskflow::domain::CredentialValidationError);
  EXPECT_NO_THROW(taskflow::domain::validate_password("correct horse battery staple"));
}

TEST(IdentitySecurity, Argon2idRoundTripUsesUniqueSaltAndRejectsWrongCredential) {
  const taskflow::infrastructure::PasswordHasher hasher{
      {.time_cost = 1, .memory_cost_kib = 8192, .parallelism = 1}};
  const auto first = hasher.hash("correct horse battery staple");
  const auto second = hasher.hash("correct horse battery staple");
  EXPECT_NE(first, second);
  EXPECT_TRUE(first.starts_with("$argon2id$v=19$"));
  EXPECT_TRUE(hasher.verify("correct horse battery staple", first));
  EXPECT_FALSE(hasher.verify("incorrect password", first));
  EXPECT_FALSE(hasher.verify("correct horse battery staple", "$argon2id$invalid"));
}
