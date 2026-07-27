#pragma once

#include "taskflow/infrastructure/notification_repository.hpp"

namespace taskflow::infrastructure {

struct ReplayBatch {
  bool resync_required;
  std::vector<NotificationEvent> events;
  std::uint64_t live_after_sequence;
};

class NotificationDelivery {
public:
  explicit NotificationDelivery(NotificationRepository &notifications);
  [[nodiscard]] ReplayBatch resume(const domain::Uuid &recipient_id, std::uint64_t after_sequence,
                                   std::size_t batch_size);
  void acknowledge(const domain::Uuid &recipient_id, std::uint64_t sequence_id,
                   std::uint64_t highest_delivered);

private:
  NotificationRepository *notifications_;
};

} // namespace taskflow::infrastructure
