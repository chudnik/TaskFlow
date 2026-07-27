#include "taskflow/infrastructure/redis_notification_wakeup.hpp"

#if TASKFLOW_HAS_REDIS
#include <sw/redis++/redis++.h>
#endif

namespace taskflow::infrastructure {

struct RedisNotificationWakeup::Impl {
#if TASKFLOW_HAS_REDIS
  explicit Impl(const std::string &uri) : redis{uri} {}
  sw::redis::Redis redis;
#else
  explicit Impl(const std::string &) {}
#endif
};

RedisNotificationWakeup::RedisNotificationWakeup(std::string uri)
    : impl_{std::make_unique<Impl>(uri)} {}
RedisNotificationWakeup::~RedisNotificationWakeup() = default;
RedisNotificationWakeup::RedisNotificationWakeup(RedisNotificationWakeup &&) noexcept = default;
RedisNotificationWakeup &
RedisNotificationWakeup::operator=(RedisNotificationWakeup &&) noexcept = default;

bool RedisNotificationWakeup::publish(const domain::Uuid &recipient_id) noexcept {
#if TASKFLOW_HAS_REDIS
  try {
    static_cast<void>(impl_->redis.publish("taskflow.notifications", recipient_id.to_string()));
    return true;
  } catch (const sw::redis::Error &) {
    return false;
  }
#else
  static_cast<void>(recipient_id);
  return false;
#endif
}

} // namespace taskflow::infrastructure
