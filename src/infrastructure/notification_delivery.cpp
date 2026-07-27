#include "taskflow/infrastructure/notification_delivery.hpp"

namespace taskflow::infrastructure {

NotificationDelivery::NotificationDelivery(NotificationRepository &notifications)
    : notifications_{&notifications} {}

ReplayBatch NotificationDelivery::resume(const domain::Uuid &recipient_id,
                                         const std::uint64_t after_sequence,
                                         const std::size_t batch_size) {
  const auto oldest = notifications_->oldest_retained(recipient_id);
  if (after_sequence != 0 && oldest && after_sequence + 1 < *oldest)
    return {true, {}, after_sequence};
  auto events = notifications_->replay(recipient_id, after_sequence, batch_size);
  const auto live_after = events.empty() ? after_sequence : events.back().sequence_id;
  return {false, std::move(events), live_after};
}

void NotificationDelivery::acknowledge(const domain::Uuid &recipient_id,
                                       const std::uint64_t sequence_id,
                                       const std::uint64_t highest_delivered) {
  if (sequence_id > highest_delivered)
    throw RepositoryError{RepositoryErrorCode::constraint_violation,
                          "cannot acknowledge an undelivered notification"};
  notifications_->acknowledge(recipient_id, sequence_id);
}

} // namespace taskflow::infrastructure
