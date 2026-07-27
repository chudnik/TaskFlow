#pragma once

#include "taskflow/application/notification_wakeup.hpp"

#include <memory>

namespace taskflow::infrastructure {

class RedisNotificationWakeup final : public application::NotificationWakeup {
public:
  explicit RedisNotificationWakeup(std::string redis_uri);
  ~RedisNotificationWakeup() override;
  RedisNotificationWakeup(RedisNotificationWakeup &&) noexcept;
  RedisNotificationWakeup &operator=(RedisNotificationWakeup &&) noexcept;
  [[nodiscard]] bool publish(const domain::Uuid &recipient_id) noexcept override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace taskflow::infrastructure
