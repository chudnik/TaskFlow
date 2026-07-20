#pragma once

#include "taskflow/application/rate_limiter.hpp"

#include <memory>
#include <string>

namespace taskflow::infrastructure {

class RedisRateLimitBackend final : public application::RateLimitBackend {
public:
  explicit RedisRateLimitBackend(std::string redis_uri);
  ~RedisRateLimitBackend() override;
  RedisRateLimitBackend(RedisRateLimitBackend &&) noexcept;
  RedisRateLimitBackend &operator=(RedisRateLimitBackend &&) noexcept;

  [[nodiscard]] std::optional<std::uint64_t>
  increment(std::string_view key, std::chrono::seconds window) noexcept override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace taskflow::infrastructure
