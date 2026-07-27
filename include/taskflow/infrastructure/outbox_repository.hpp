#pragma once

#include "taskflow/domain/common.hpp"
#include "taskflow/infrastructure/postgres.hpp"

namespace taskflow::infrastructure {

struct OutboxEvent {
  domain::Uuid event_id;
  std::optional<domain::Uuid> project_id;
  std::string aggregate_type;
  domain::Uuid aggregate_id;
  std::string event_type;
  std::string payload;
  std::string correlation_id;
  std::size_t attempts;
};

class OutboxRepository {
public:
  explicit OutboxRepository(PostgresConnection &connection);
  [[nodiscard]] std::vector<OutboxEvent> claim(std::string worker_id, std::size_t batch_size,
                                               std::chrono::seconds lease_duration);
  void mark_processed(const domain::Uuid &event_id, std::string_view worker_id);
  void release_for_retry(const domain::Uuid &event_id, std::string_view worker_id,
                         std::string error, std::chrono::seconds backoff);
  [[nodiscard]] std::size_t cleanup_processed(std::chrono::hours retention);

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
