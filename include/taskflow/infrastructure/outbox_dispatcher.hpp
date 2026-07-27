#pragma once

#include "taskflow/application/notification_wakeup.hpp"
#include "taskflow/infrastructure/notification_repository.hpp"
#include "taskflow/infrastructure/outbox_repository.hpp"

namespace taskflow::infrastructure {

class OutboxDispatcher {
public:
  OutboxDispatcher(OutboxRepository &outbox, NotificationRepository &notifications,
                   application::WakeupCoordinator &wakeups, std::string worker_id,
                   std::size_t batch_size, std::chrono::seconds lease_duration);
  [[nodiscard]] std::size_t run_once();

private:
  OutboxRepository *outbox_;
  NotificationRepository *notifications_;
  application::WakeupCoordinator *wakeups_;
  std::string worker_id_;
  std::size_t batch_size_;
  std::chrono::seconds lease_duration_;
};

} // namespace taskflow::infrastructure
