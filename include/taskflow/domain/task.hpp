#pragma once

#include "taskflow/domain/common.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace taskflow::domain {

enum class TaskStatus { todo, in_progress, done, cancelled };
enum class TaskPriority { low, medium, high, urgent };

struct Task {
  Uuid id;
  Uuid project_id;
  std::string title;
  std::string description;
  TaskStatus status;
  TaskPriority priority;
  Uuid creator_id;
  std::optional<Uuid> assignee_id;
  std::optional<UtcInstant> deadline_at;
  std::optional<UtcInstant> completed_at;
  std::uint64_t version;
  UtcInstant created_at;
  UtcInstant updated_at;
  std::optional<UtcInstant> deleted_at;
  std::optional<Uuid> deleted_by;

  [[nodiscard]] bool deleted() const noexcept { return deleted_at.has_value(); }
  [[nodiscard]] bool overdue(UtcInstant at) const noexcept;
};

[[nodiscard]] ValidationErrors validate_task_fields(std::string_view title,
                                                    std::string_view description);
[[nodiscard]] bool can_transition(TaskStatus from, TaskStatus to) noexcept;
[[nodiscard]] bool can_assign(std::optional<Uuid> assignee_id,
                              bool assignee_is_active_member) noexcept;
[[nodiscard]] bool apply_transition(Task &task, TaskStatus next, UtcInstant at) noexcept;
[[nodiscard]] bool assign(Task &task, std::optional<Uuid> assignee_id,
                          bool assignee_is_active_member, UtcInstant at) noexcept;
[[nodiscard]] bool soft_delete(Task &task, const Uuid &actor_id, UtcInstant at) noexcept;
[[nodiscard]] std::string_view task_status_name(TaskStatus status) noexcept;
[[nodiscard]] std::optional<TaskStatus> parse_task_status(std::string_view value) noexcept;
[[nodiscard]] std::string_view task_priority_name(TaskPriority priority) noexcept;
[[nodiscard]] std::optional<TaskPriority> parse_task_priority(std::string_view value) noexcept;

} // namespace taskflow::domain
