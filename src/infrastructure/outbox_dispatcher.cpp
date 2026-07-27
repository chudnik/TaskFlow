#include "taskflow/infrastructure/outbox_dispatcher.hpp"

#include <algorithm>

namespace taskflow::infrastructure {

OutboxDispatcher::OutboxDispatcher(OutboxRepository &outbox, NotificationRepository &notifications,
                                   application::WakeupCoordinator &wakeups, std::string worker_id,
                                   const std::size_t batch_size,
                                   const std::chrono::seconds lease_duration)
    : outbox_{&outbox}, notifications_{&notifications}, wakeups_{&wakeups},
      worker_id_{std::move(worker_id)}, batch_size_{batch_size}, lease_duration_{lease_duration} {}

std::size_t OutboxDispatcher::run_once() {
  const auto events = outbox_->claim(worker_id_, batch_size_, lease_duration_);
  for (const auto &event : events) {
    try {
      const auto recipients =
          notifications_->materialize_recipients(event, std::chrono::hours{24 * 30});
      for (const auto &recipient : recipients)
        static_cast<void>(wakeups_->notify(recipient));
      outbox_->mark_processed(event.event_id, worker_id_);
    } catch (const std::exception &error) {
      const auto exponent = std::min<std::size_t>(event.attempts, 8);
      outbox_->release_for_retry(event.event_id, worker_id_, error.what(),
                                 std::chrono::seconds{1ULL << exponent});
    }
  }
  return events.size();
}

} // namespace taskflow::infrastructure
