#include "taskflow/infrastructure/outbox_repository.hpp"

namespace taskflow::infrastructure {
namespace {

const std::string &required(const QueryResult &result, const std::size_t row,
                            const std::size_t column) {
  const auto &value = result.value(row, column);
  if (!value)
    throw RepositoryError{RepositoryErrorCode::unexpected, "outbox row contains null"};
  return *value;
}

domain::Uuid uuid(const std::string &value) {
  const auto parsed = domain::Uuid::parse(value);
  if (!parsed)
    throw RepositoryError{RepositoryErrorCode::unexpected, "outbox UUID is invalid"};
  return *parsed;
}

} // namespace

OutboxRepository::OutboxRepository(PostgresConnection &connection) : connection_{&connection} {}

std::vector<OutboxEvent> OutboxRepository::claim(std::string worker_id,
                                                 const std::size_t batch_size,
                                                 const std::chrono::seconds lease_duration) {
  auto transaction = connection_->transaction();
  const auto result = transaction.execute(
      "WITH candidates AS (SELECT event_id FROM outbox_events "
      "WHERE processed_at IS NULL AND available_at <= clock_timestamp() "
      "AND (locked_until IS NULL OR locked_until < clock_timestamp()) "
      "ORDER BY available_at, occurred_at, event_id FOR UPDATE SKIP LOCKED LIMIT $1::bigint) "
      "UPDATE outbox_events o SET locked_by = $2, "
      "locked_until = clock_timestamp() + ($3::bigint * interval '1 second'), "
      "attempts = attempts + 1 FROM candidates c WHERE o.event_id = c.event_id "
      "RETURNING o.event_id::text, o.project_id::text, o.aggregate_type, "
      "o.aggregate_id::text, o.event_type, o.payload::text, o.correlation_id, "
      "o.attempts::text",
      {std::to_string(batch_size), worker_id, std::to_string(lease_duration.count())});
  transaction.commit();
  std::vector<OutboxEvent> events;
  events.reserve(result.row_count());
  for (std::size_t row = 0; row < result.row_count(); ++row) {
    events.push_back(
        {uuid(required(result, row, 0)),
         result.value(row, 1) ? std::optional{uuid(*result.value(row, 1))} : std::nullopt,
         required(result, row, 2), uuid(required(result, row, 3)), required(result, row, 4),
         required(result, row, 5), required(result, row, 6),
         std::stoull(required(result, row, 7))});
  }
  return events;
}

void OutboxRepository::mark_processed(const domain::Uuid &event_id,
                                      const std::string_view worker_id) {
  static_cast<void>(connection_->execute(
      "UPDATE outbox_events SET processed_at = clock_timestamp(), locked_by = NULL, "
      "locked_until = NULL, last_error = NULL WHERE event_id = $1::uuid "
      "AND locked_by = $2 AND processed_at IS NULL",
      {event_id.to_string(), std::string{worker_id}}));
}

void OutboxRepository::release_for_retry(const domain::Uuid &event_id,
                                         const std::string_view worker_id, std::string error,
                                         const std::chrono::seconds backoff) {
  static_cast<void>(connection_->execute(
      "UPDATE outbox_events SET locked_by = NULL, locked_until = NULL, last_error = $3, "
      "available_at = clock_timestamp() + ($4::bigint * interval '1 second') "
      "WHERE event_id = $1::uuid AND locked_by = $2 AND processed_at IS NULL",
      {event_id.to_string(), std::string{worker_id}, std::move(error),
       std::to_string(backoff.count())}));
}

std::size_t OutboxRepository::cleanup_processed(const std::chrono::hours retention) {
  return connection_
      ->execute("DELETE FROM outbox_events WHERE processed_at < clock_timestamp() - "
                "($1::bigint * interval '1 hour')",
                {std::to_string(retention.count())})
      .affected_rows();
}

} // namespace taskflow::infrastructure
