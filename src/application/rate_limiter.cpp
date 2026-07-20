#include "taskflow/application/rate_limiter.hpp"

#include <stdexcept>
#include <string>

namespace taskflow::application {

AuthenticationRateLimiter::AuthenticationRateLimiter(RateLimitBackend &backend,
                                                       const RateLimitPolicy policy)
    : backend_{&backend}, policy_{policy} {
  if (policy_.login_attempts == 0 || policy_.refresh_attempts == 0 ||
      policy_.window.count() <= 0) {
    throw std::invalid_argument{"rate limit values must be positive"};
  }
}

RateLimitDecision AuthenticationRateLimiter::check(
    const RateLimitOperation operation, const std::string_view client_key) const noexcept {
  if (client_key.empty()) {
    return RateLimitDecision::service_unavailable;
  }
  const auto prefix = operation == RateLimitOperation::login ? "login:" : "refresh:";
  const auto count = backend_->increment(std::string{prefix} + std::string{client_key},
                                         policy_.window);
  if (!count) {
    // Authentication endpoints fail closed while Redis is degraded.
    return RateLimitDecision::service_unavailable;
  }
  const auto maximum = operation == RateLimitOperation::login ? policy_.login_attempts
                                                               : policy_.refresh_attempts;
  return *count <= maximum ? RateLimitDecision::allowed : RateLimitDecision::limited;
}

} // namespace taskflow::application
