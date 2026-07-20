#include "taskflow/infrastructure/redis_rate_limit.hpp"

#if TASKFLOW_HAS_REDIS
#include <sw/redis++/redis++.h>
#endif

#include <string>
#include <utility>

namespace taskflow::infrastructure {

struct RedisRateLimitBackend::Impl {
#if TASKFLOW_HAS_REDIS
  explicit Impl(const std::string &uri) : redis{uri} {}
  sw::redis::Redis redis;
#else
  explicit Impl(const std::string &) {}
#endif
};

RedisRateLimitBackend::RedisRateLimitBackend(std::string redis_uri)
    : impl_{std::make_unique<Impl>(redis_uri)} {}
RedisRateLimitBackend::~RedisRateLimitBackend() = default;
RedisRateLimitBackend::RedisRateLimitBackend(RedisRateLimitBackend &&) noexcept = default;
RedisRateLimitBackend &RedisRateLimitBackend::operator=(RedisRateLimitBackend &&) noexcept = default;

std::optional<std::uint64_t>
RedisRateLimitBackend::increment(const std::string_view key,
                                 const std::chrono::seconds window) noexcept {
#if TASKFLOW_HAS_REDIS
  try {
    static constexpr std::string_view script =
        "local count = redis.call('INCR', KEYS[1]); "
        "if count == 1 then redis.call('EXPIRE', KEYS[1], ARGV[1]); end; return count";
    const auto seconds = std::to_string(window.count());
    const auto count = impl_->redis.eval<long long>(script, {key}, {seconds});
    return count < 0 ? std::nullopt
                     : std::optional<std::uint64_t>{static_cast<std::uint64_t>(count)};
  } catch (const sw::redis::Error &) {
    return std::nullopt;
  }
#else
  (void)key;
  (void)window;
  return std::nullopt;
#endif
}

} // namespace taskflow::infrastructure
