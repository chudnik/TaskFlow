#include "taskflow/application/rate_limiter.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow::application;

class Backend final : public RateLimitBackend {
public:
  std::optional<std::uint64_t> next{1};
  std::optional<std::uint64_t> increment(std::string_view,
                                         std::chrono::seconds) noexcept override {
    return next;
  }
};

TEST(AuthenticationRateLimiter, AppliesIndependentConfigurableLimits) {
  Backend backend;
  const AuthenticationRateLimiter limiter{backend, {3, 5, std::chrono::minutes{1}}};
  backend.next = 3;
  EXPECT_EQ(limiter.check(RateLimitOperation::login, "client"), RateLimitDecision::allowed);
  backend.next = 4;
  EXPECT_EQ(limiter.check(RateLimitOperation::login, "client"), RateLimitDecision::limited);
  EXPECT_EQ(limiter.check(RateLimitOperation::refresh, "client"), RateLimitDecision::allowed);
  backend.next = 6;
  EXPECT_EQ(limiter.check(RateLimitOperation::refresh, "client"), RateLimitDecision::limited);
}

TEST(AuthenticationRateLimiter, FailsClosedWhenRedisIsUnavailable) {
  Backend backend;
  backend.next = std::nullopt;
  const AuthenticationRateLimiter limiter{backend, {3, 5, std::chrono::minutes{1}}};
  EXPECT_EQ(limiter.check(RateLimitOperation::login, "client"),
            RateLimitDecision::service_unavailable);
  EXPECT_EQ(limiter.check(RateLimitOperation::refresh, "client"),
            RateLimitDecision::service_unavailable);
}
} // namespace
