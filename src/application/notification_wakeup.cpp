#include "taskflow/application/notification_wakeup.hpp"

namespace taskflow::application {

WakeupCoordinator::WakeupCoordinator(NotificationWakeup &wakeup) : wakeup_{&wakeup} {}

bool WakeupCoordinator::notify(const domain::Uuid &recipient_id) noexcept {
  if (!wakeup_->publish(recipient_id)) {
    fallback_ = true;
    return false;
  }
  return true;
}

bool WakeupCoordinator::postgres_poll_required() const noexcept { return fallback_; }
void WakeupCoordinator::redis_recovered() noexcept { fallback_ = false; }

} // namespace taskflow::application
