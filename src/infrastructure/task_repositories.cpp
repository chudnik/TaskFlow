#include "taskflow/infrastructure/task_repositories.hpp"
#include "taskflow/infrastructure/task_query_sql.hpp"

namespace taskflow::infrastructure {
namespace {

[[nodiscard]] const std::string &required(const QueryResult &result, const std::size_t column,
                                          const std::size_t row = 0) {
  const auto &value = result.value(row, column);
  if (!value) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "task row contains null"};
  }
  return *value;
}

[[nodiscard]] domain::Uuid uuid(const std::string &value) {
  const auto parsed = domain::Uuid::parse(value);
  if (!parsed) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "task row contains invalid UUID"};
  }
  return *parsed;
}

[[nodiscard]] domain::UtcInstant instant(const std::string &value) {
  const auto parsed = domain::parse_utc(value);
  if (!parsed) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "task row contains invalid timestamp"};
  }
  return *parsed;
}

constexpr std::string_view columns =
    "id::text, project_id::text, title, description, status, priority, creator_id::text, "
    "assignee_id::text, "
    "CASE WHEN deadline_at IS NULL THEN NULL ELSE "
    "to_char(deadline_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') END, "
    "CASE WHEN completed_at IS NULL THEN NULL ELSE "
    "to_char(completed_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') END, "
    "version::text, "
    "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "CASE WHEN deleted_at IS NULL THEN NULL ELSE "
    "to_char(deleted_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') END, "
    "deleted_by::text";

[[nodiscard]] domain::Task task_from(const QueryResult &result, const std::size_t row = 0) {
  const auto status = domain::parse_task_status(required(result, 4, row));
  const auto priority = domain::parse_task_priority(required(result, 5, row));
  if (!status || !priority) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "task row contains invalid enum"};
  }
  return {uuid(required(result, 0, row)),
          uuid(required(result, 1, row)),
          required(result, 2, row),
          required(result, 3, row),
          *status,
          *priority,
          uuid(required(result, 6, row)),
          result.value(row, 7) ? std::optional{uuid(*result.value(row, 7))} : std::nullopt,
          result.value(row, 8) ? std::optional{instant(*result.value(row, 8))} : std::nullopt,
          result.value(row, 9) ? std::optional{instant(*result.value(row, 9))} : std::nullopt,
          std::stoull(required(result, 10, row)),
          instant(required(result, 11, row)),
          instant(required(result, 12, row)),
          result.value(row, 13) ? std::optional{instant(*result.value(row, 13))} : std::nullopt,
          result.value(row, 14) ? std::optional{uuid(*result.value(row, 14))} : std::nullopt};
}

[[noreturn]] void stale_or_missing() {
  throw RepositoryError{RepositoryErrorCode::conflict,
                        "task version is stale or task is unavailable"};
}

} // namespace

TaskRepository::TaskRepository(PostgresConnection &connection) : connection_{&connection} {}

domain::Task TaskRepository::create(const domain::Uuid &project_id, std::string title,
                                    std::string description, const domain::TaskPriority priority,
                                    const domain::Uuid &creator_id,
                                    const std::optional<domain::Uuid> assignee_id,
                                    const std::optional<domain::UtcInstant> deadline_at) {
  const auto result = connection_->execute(
      "INSERT INTO tasks(id, project_id, title, description, priority, creator_id, "
      "assignee_id, deadline_at) VALUES($1::uuid, $2::uuid, $3, $4, $5, $6::uuid, "
      "$7::uuid, $8::timestamptz) RETURNING " +
          std::string{columns},
      {domain::Uuid::generate().to_string(), project_id.to_string(), std::move(title),
       std::move(description), std::string{domain::task_priority_name(priority)},
       creator_id.to_string(),
       assignee_id ? QueryParameter{assignee_id->to_string()} : std::nullopt,
       deadline_at ? QueryParameter{domain::format_utc(*deadline_at)} : std::nullopt});
  return task_from(result);
}

domain::Task TaskRepository::create_task(const domain::Uuid &project_id, std::string title,
                                         std::string description,
                                         const domain::TaskPriority priority,
                                         const domain::Uuid &creator_id,
                                         const std::optional<domain::Uuid> assignee_id,
                                         const std::optional<domain::UtcInstant> deadline_at) {
  return create(project_id, std::move(title), std::move(description), priority, creator_id,
                assignee_id, deadline_at);
}

std::optional<domain::Task> TaskRepository::find_active(const domain::Uuid &task_id) {
  const auto result = connection_->execute("SELECT " + std::string{columns} +
                                               " FROM tasks WHERE id = $1::uuid "
                                               "AND deleted_at IS NULL",
                                           {task_id.to_string()});
  return result.row_count() == 0 ? std::nullopt : std::optional{task_from(result)};
}

std::optional<domain::Task> TaskRepository::find_active_task(const domain::Uuid &task_id) {
  return find_active(task_id);
}

domain::Task TaskRepository::update(const domain::Task &task,
                                    const std::uint64_t expected_version) {
  const auto result = connection_->execute(
      "UPDATE tasks SET title = $3, description = $4, status = $5, priority = $6, "
      "assignee_id = $7::uuid, deadline_at = $8::timestamptz, "
      "completed_at = $9::timestamptz, version = version + 1, "
      "updated_at = clock_timestamp() WHERE id = $1::uuid AND version = $2::bigint "
      "AND deleted_at IS NULL RETURNING " +
          std::string{columns},
      {task.id.to_string(), std::to_string(expected_version), task.title, task.description,
       std::string{domain::task_status_name(task.status)},
       std::string{domain::task_priority_name(task.priority)},
       task.assignee_id ? QueryParameter{task.assignee_id->to_string()} : std::nullopt,
       task.deadline_at ? QueryParameter{domain::format_utc(*task.deadline_at)} : std::nullopt,
       task.completed_at ? QueryParameter{domain::format_utc(*task.completed_at)} : std::nullopt});
  if (result.row_count() == 0) {
    stale_or_missing();
  }
  return task_from(result);
}

domain::Task TaskRepository::update_task(const domain::Task &task,
                                         const std::uint64_t expected_version) {
  return update(task, expected_version);
}

domain::Task TaskRepository::soft_delete(const domain::Uuid &task_id,
                                         const std::uint64_t expected_version,
                                         const domain::Uuid &actor_id) {
  const auto result = connection_->execute(
      "UPDATE tasks SET deleted_at = clock_timestamp(), deleted_by = $3::uuid, "
      "version = version + 1, updated_at = clock_timestamp() "
      "WHERE id = $1::uuid AND version = $2::bigint AND deleted_at IS NULL RETURNING " +
          std::string{columns},
      {task_id.to_string(), std::to_string(expected_version), actor_id.to_string()});
  if (result.row_count() == 0) {
    stale_or_missing();
  }
  return task_from(result);
}

domain::Task TaskRepository::delete_task(const domain::Uuid &task_id,
                                         const std::uint64_t expected_version,
                                         const domain::Uuid &actor_id) {
  return soft_delete(task_id, expected_version, actor_id);
}

std::vector<domain::Task>
TaskRepository::list_tasks(const application::NormalizedTaskQuery &query,
                           const domain::Uuid &caller_id,
                           const std::optional<application::TaskCursor> after,
                           const std::size_t limit, const domain::UtcInstant now) {
  auto statement = build_task_list_query(query, caller_id, now, limit, after);
  statement.sql.replace(0, std::string{"SELECT t.*"}.size(), "SELECT " + std::string{columns});
  const auto result = connection_->execute(statement.sql, statement.parameters);
  std::vector<domain::Task> tasks;
  tasks.reserve(result.row_count());
  for (std::size_t row = 0; row < result.row_count(); ++row)
    tasks.push_back(task_from(result, row));
  return tasks;
}

} // namespace taskflow::infrastructure
