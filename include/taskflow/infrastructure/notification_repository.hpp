#pragma once

#include "taskflow/infrastructure/outbox_repository.hpp"

namespace taskflow::infrastructure {

struct NotificationEvent {
  std::uint64_t sequence_id;
  domain::Uuid event_id;
  domain::Uuid recipient_user_id;
  std::optional<domain::Uuid> project_id;
  std::string event_type;
  std::optional<domain::Uuid> entity_id;
  std::string payload;
};

class NotificationRepository {
public:
  explicit NotificationRepository(PostgresConnection &connection);
  [[nodiscard]] std::size_t materialize(const OutboxEvent &event, std::chrono::hours retention);
  [[nodiscard]] std::vector<domain::Uuid> materialize_recipients(const OutboxEvent &event,
                                                                 std::chrono::hours retention);
  [[nodiscard]] std::vector<NotificationEvent>
  replay(const domain::Uuid &recipient_id, std::uint64_t after_sequence, std::size_t limit);
  void acknowledge(const domain::Uuid &recipient_id, std::uint64_t through_sequence);
  [[nodiscard]] std::optional<std::uint64_t> oldest_retained(const domain::Uuid &recipient_id);

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
