#include "taskflow/infrastructure/refresh_tokens.hpp"

#include <gtest/gtest.h>

TEST(RefreshTokens, GeneratesOpaqueHighEntropyTokensAndStableDigests) {
  const auto first = taskflow::infrastructure::RefreshTokenService::generate_token();
  const auto second = taskflow::infrastructure::RefreshTokenService::generate_token();
  EXPECT_EQ(first.size(), 43U);
  EXPECT_NE(first, second);
  const auto digest = taskflow::infrastructure::RefreshTokenService::hash_token(first);
  EXPECT_EQ(digest.size(), 64U);
  EXPECT_EQ(digest, taskflow::infrastructure::RefreshTokenService::hash_token(first));
  EXPECT_NE(digest, taskflow::infrastructure::RefreshTokenService::hash_token(second));
  EXPECT_EQ(digest.find(first), std::string::npos);
}
