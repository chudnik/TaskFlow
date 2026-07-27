#pragma once

#include "taskflow/domain/common.hpp"

namespace taskflow::application {

class NotificationWakeup {
public:
  virtual ~NotificationWakeup() = default;
  [[nodiscard]] virtual bool publish(const domain::Uuid &recipient_id) noexcept = 0;
};

class WakeupCoordinator {
public:
  explicit WakeupCoordinator(NotificationWakeup &wakeup);
  [[nodiscard]] bool notify(const domain::Uuid &recipient_id) noexcept;
  [[nodiscard]] bool postgres_poll_required() const noexcept;
  void redis_recovered() noexcept;

private:
  NotificationWakeup *wakeup_;
  bool fallback_{false};
};

} // namespace taskflow::application
