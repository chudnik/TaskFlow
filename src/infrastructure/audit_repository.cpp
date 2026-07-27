#include "taskflow/infrastructure/audit_repository.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::infrastructure {
namespace {

nlohmann::json json(const std::map<std::string, std::string> &fields) {
  nlohmann::json result = nlohmann::json::object();
  for (const auto &[key, value] : fields)
    result[key] = value;
  return result;
}

std::map<std::string, std::string> fields(const std::string &value) {
  std::map<std::string, std::string> result;
  const auto document = nlohmann::json::parse(value);
  for (const auto &[key, item] : document.items())
    result.emplace(key, item.is_string() ? item.get<std::string>() : item.dump());
  return result;
}

const std::string &required(const QueryResult &result, const std::size_t row,
                            const std::size_t column) {
  const auto &value = result.value(row, column);
  if (!value)
    throw RepositoryError{RepositoryErrorCode::unexpected, "audit row contains null"};
  return *value;
}

domain::Uuid uuid(const std::string &value) {
  const auto parsed = domain::Uuid::parse(value);
  if (!parsed)
    throw RepositoryError{RepositoryErrorCode::unexpected, "audit UUID is invalid"};
  return *parsed;
}

domain::UtcInstant instant(const std::string &value) {
  const auto parsed = domain::parse_utc(value);
  if (!parsed)
    throw RepositoryError{RepositoryErrorCode::unexpected, "audit timestamp is invalid"};
  return *parsed;
}

constexpr std::string_view columns =
    "id::text, event_id::text, project_id::text, task_id::text, actor_user_id::text, "
    "event_type, entity_type, entity_id::text, "
    "COALESCE(before_data, '{}'::jsonb)::text, COALESCE(after_data, '{}'::jsonb)::text, "
    "correlation_id, "
    "to_char(occurred_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"')";

application::AuditEvent event_from(const QueryResult &result, const std::size_t row = 0) {
  return {std::stoull(required(result, row, 0)),
          uuid(required(result, row, 1)),
          uuid(required(result, row, 2)),
          result.value(row, 3) ? std::optional{uuid(*result.value(row, 3))} : std::nullopt,
          result.value(row, 4) ? std::optional{uuid(*result.value(row, 4))} : std::nullopt,
          required(result, row, 5),
          required(result, row, 6),
          uuid(required(result, row, 7)),
          fields(required(result, row, 8)),
          fields(required(result, row, 9)),
          required(result, row, 10),
          instant(required(result, row, 11))};
}

} // namespace

AuditRepository::AuditRepository(PostgresConnection &connection) : connection_{&connection} {}

application::AuditEvent AuditRepository::append_audit(application::AuditEvent event) {
  event.before = application::sanitize_audit_fields(std::move(event.before));
  event.after = application::sanitize_audit_fields(std::move(event.after));
  const auto result = connection_->execute(
      "INSERT INTO audit_events(event_id, project_id, task_id, actor_type, actor_user_id, "
      "event_type, entity_type, entity_id, before_data, after_data, correlation_id) "
      "VALUES($1::uuid,$2::uuid,$3::uuid,$4,$5::uuid,$6,$7,$8::uuid,$9::jsonb,$10::jsonb,$11) "
      "RETURNING " +
          std::string{columns},
      {event.event_id.to_string(), event.project_id.to_string(),
       event.task_id ? QueryParameter{event.task_id->to_string()} : std::nullopt,
       event.actor_user_id ? "user" : "system",
       event.actor_user_id ? QueryParameter{event.actor_user_id->to_string()} : std::nullopt,
       event.event_type, event.entity_type, event.entity_id.to_string(), json(event.before).dump(),
       json(event.after).dump(), event.correlation_id});
  return event_from(result);
}

application::AuditPage AuditRepository::list_audit(const domain::Uuid &project_id,
                                                   const std::optional<domain::Uuid> task_id,
                                                   const std::optional<std::uint64_t> before_id,
                                                   const std::size_t limit) {
  const auto result = connection_->execute(
      "SELECT " + std::string{columns} +
          " FROM audit_events WHERE project_id = $1::uuid "
          "AND ($2::uuid IS NULL OR task_id = $2::uuid) "
          "AND ($3::bigint IS NULL OR id < $3::bigint) ORDER BY id DESC LIMIT $4::bigint",
      {project_id.to_string(), task_id ? QueryParameter{task_id->to_string()} : std::nullopt,
       before_id ? QueryParameter{std::to_string(*before_id)} : std::nullopt,
       std::to_string(limit + 1)});
  application::AuditPage page;
  const auto count = std::min(result.row_count(), limit);
  page.items.reserve(count);
  for (std::size_t row = 0; row < count; ++row)
    page.items.push_back(event_from(result, row));
  if (result.row_count() > limit)
    page.next_before_id = page.items.back().id;
  return page;
}

} // namespace taskflow::infrastructure
