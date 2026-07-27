#include "taskflow/infrastructure/notification_repository.hpp"

#include <algorithm>

namespace taskflow::infrastructure {
namespace {

const std::string &required(const QueryResult &result, const std::size_t row,
                            const std::size_t column) {
  const auto &value = result.value(row, column);
  if (!value)
    throw RepositoryError{RepositoryErrorCode::unexpected, "notification row contains null"};
  return *value;
}

domain::Uuid uuid(const std::string &value) {
  const auto parsed = domain::Uuid::parse(value);
  if (!parsed)
    throw RepositoryError{RepositoryErrorCode::unexpected, "notification UUID is invalid"};
  return *parsed;
}

} // namespace

NotificationRepository::NotificationRepository(PostgresConnection &connection)
    : connection_{&connection} {}

std::size_t NotificationRepository::materialize(const OutboxEvent &event,
                                                const std::chrono::hours retention) {
  return materialize_recipients(event, retention).size();
}

std::vector<domain::Uuid>
NotificationRepository::materialize_recipients(const OutboxEvent &event,
                                               const std::chrono::hours retention) {
  if (!event.project_id)
    return {};
  const auto result = connection_->execute(
      "INSERT INTO notification_events(event_id, source_event_id, recipient_user_id, "
      "project_id, event_type, entity_id, payload, expires_at) "
      "SELECT $1::uuid, $1::uuid, pm.user_id, $2::uuid, $3, $4::uuid, $5::jsonb, "
      "clock_timestamp() + ($6::bigint * interval '1 hour') "
      "FROM project_members pm WHERE pm.project_id = $2::uuid "
      "AND ($3 NOT IN ('task.pre_deadline','task.overdue') "
      "OR pm.user_id = ($5::jsonb->>'recipient_id')::uuid) "
      "ON CONFLICT(recipient_user_id, event_id) DO NOTHING RETURNING recipient_user_id::text",
      {event.event_id.to_string(), event.project_id->to_string(), event.event_type,
       event.aggregate_id.to_string(), event.payload, std::to_string(retention.count())});
  std::vector<domain::Uuid> recipients;
  recipients.reserve(result.row_count());
  for (std::size_t row = 0; row < result.row_count(); ++row)
    recipients.push_back(uuid(required(result, row, 0)));
  return recipients;
}

std::vector<NotificationEvent> NotificationRepository::replay(const domain::Uuid &recipient_id,
                                                              const std::uint64_t after_sequence,
                                                              const std::size_t limit) {
  const auto result = connection_->execute(
      "SELECT sequence_id::text, event_id::text, recipient_user_id::text, project_id::text, "
      "event_type, entity_id::text, payload::text FROM notification_events "
      "WHERE recipient_user_id = $1::uuid AND sequence_id > $2::bigint "
      "AND expires_at > clock_timestamp() "
      "AND (project_id IS NULL OR EXISTS (SELECT 1 FROM project_members pm "
      "WHERE pm.project_id = notification_events.project_id "
      "AND pm.user_id = notification_events.recipient_user_id)) "
      "ORDER BY sequence_id LIMIT $3::bigint",
      {recipient_id.to_string(), std::to_string(after_sequence), std::to_string(limit)});
  std::vector<NotificationEvent> events;
  events.reserve(result.row_count());
  for (std::size_t row = 0; row < result.row_count(); ++row)
    events.push_back(
        {std::stoull(required(result, row, 0)), uuid(required(result, row, 1)),
         uuid(required(result, row, 2)),
         result.value(row, 3) ? std::optional{uuid(*result.value(row, 3))} : std::nullopt,
         required(result, row, 4),
         result.value(row, 5) ? std::optional{uuid(*result.value(row, 5))} : std::nullopt,
         required(result, row, 6)});
  std::sort(events.begin(), events.end(), [](const auto &left, const auto &right) {
    return left.sequence_id < right.sequence_id;
  });
  return events;
}

void NotificationRepository::acknowledge(const domain::Uuid &recipient_id,
                                         const std::uint64_t through_sequence) {
  static_cast<void>(connection_->execute(
      "UPDATE notification_events SET acknowledged_at = COALESCE(acknowledged_at, "
      "clock_timestamp()) WHERE recipient_user_id = $1::uuid "
      "AND sequence_id <= $2::bigint",
      {recipient_id.to_string(), std::to_string(through_sequence)}));
}

std::optional<std::uint64_t>
NotificationRepository::oldest_retained(const domain::Uuid &recipient_id) {
  const auto result =
      connection_->execute("SELECT min(sequence_id)::text FROM notification_events "
                           "WHERE recipient_user_id = $1::uuid AND expires_at > clock_timestamp()",
                           {recipient_id.to_string()});
  return result.row_count() == 0 || !result.value(0, 0)
             ? std::nullopt
             : std::optional{std::stoull(*result.value(0, 0))};
}

} // namespace taskflow::infrastructure
