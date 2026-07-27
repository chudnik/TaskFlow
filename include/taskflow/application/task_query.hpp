#pragma once

#include "taskflow/domain/task.hpp"
#include <optional>
#include <string>

namespace taskflow::application {

enum class TaskSortField { created_at, updated_at, deadline, priority, title };
enum class SortDirection { ascending, descending };

struct TaskFilters {
  domain::Uuid project_id;
  std::optional<domain::TaskStatus> status;
  std::optional<domain::TaskPriority> priority;
  std::optional<domain::Uuid> assignee_id;
  std::optional<domain::Uuid> creator_id;
  std::optional<domain::UtcInstant> deadline_from;
  std::optional<domain::UtcInstant> deadline_to;
  std::optional<bool> overdue;
  std::optional<std::string> title;
};

struct TaskOrder {
  TaskSortField field{TaskSortField::created_at};
  SortDirection direction{SortDirection::descending};
};

struct NormalizedTaskQuery {
  TaskFilters filters;
  TaskOrder order;
};

[[nodiscard]] std::optional<TaskSortField> parse_task_sort_field(std::string_view value) noexcept;
[[nodiscard]] std::optional<SortDirection> parse_sort_direction(std::string_view value) noexcept;
[[nodiscard]] NormalizedTaskQuery normalize_task_query(TaskFilters filters, TaskOrder order);
[[nodiscard]] std::string task_query_fingerprint(const NormalizedTaskQuery &query);

} // namespace taskflow::application
