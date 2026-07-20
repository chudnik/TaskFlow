#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

namespace taskflow::application {

enum class RateLimitOperation { login, refresh };
enum class RateLimitDecision { allowed, limited, service_unavailable };

class RateLimitBackend {
public:
  virtual ~RateLimitBackend() = default;
  [[nodiscard]] virtual std::optional<std::uint64_t>
  increment(std::string_view key, std::chrono::seconds window) noexcept = 0;
};

struct RateLimitPolicy {
  std::uint64_t login_attempts;
  std::uint64_t refresh_attempts;
  std::chrono::seconds window;
};

class AuthenticationRateLimiter {
public:
  AuthenticationRateLimiter(RateLimitBackend &backend, RateLimitPolicy policy);
  [[nodiscard]] RateLimitDecision check(RateLimitOperation operation,
                                        std::string_view client_key) const noexcept;

private:
  RateLimitBackend *backend_;
  RateLimitPolicy policy_;
};

} // namespace taskflow::application
