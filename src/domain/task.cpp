#include "taskflow/domain/task.hpp"

namespace taskflow::domain {

bool Task::overdue(const UtcInstant at) const noexcept {
  return !deleted() && deadline_at && *deadline_at < at && status != TaskStatus::done &&
         status != TaskStatus::cancelled;
}

ValidationErrors validate_task_fields(const std::string_view title,
                                      const std::string_view description) {
  ValidationErrors errors;
  errors.require_text("title", title, 1, 500);
  if (description.size() > 50'000) {
    errors.add("description", "too_long", "description must contain at most 50000 bytes");
  }
  return errors;
}

bool can_transition(const TaskStatus from, const TaskStatus to) noexcept {
  switch (from) {
  case TaskStatus::todo:
    return to == TaskStatus::in_progress || to == TaskStatus::cancelled;
  case TaskStatus::in_progress:
    return to == TaskStatus::todo || to == TaskStatus::done || to == TaskStatus::cancelled;
  case TaskStatus::done:
  case TaskStatus::cancelled:
    return to == TaskStatus::todo;
  }
  return false;
}

bool can_assign(const std::optional<Uuid> assignee_id,
                const bool assignee_is_active_member) noexcept {
  return !assignee_id || assignee_is_active_member;
}

bool apply_transition(Task &task, const TaskStatus next, const UtcInstant at) noexcept {
  if (task.deleted() || !can_transition(task.status, next)) {
    return false;
  }
  task.status = next;
  task.completed_at = next == TaskStatus::done ? std::optional{at} : std::nullopt;
  task.updated_at = at;
  ++task.version;
  return true;
}

bool assign(Task &task, const std::optional<Uuid> assignee_id, const bool assignee_is_active_member,
            const UtcInstant at) noexcept {
  if (task.deleted() || !can_assign(assignee_id, assignee_is_active_member)) {
    return false;
  }
  task.assignee_id = assignee_id;
  task.updated_at = at;
  ++task.version;
  return true;
}

bool soft_delete(Task &task, const Uuid &actor_id, const UtcInstant at) noexcept {
  if (task.deleted()) {
    return false;
  }
  task.deleted_at = at;
  task.deleted_by = actor_id;
  task.updated_at = at;
  ++task.version;
  return true;
}

std::string_view task_status_name(const TaskStatus status) noexcept {
  switch (status) {
  case TaskStatus::todo:
    return "todo";
  case TaskStatus::in_progress:
    return "in_progress";
  case TaskStatus::done:
    return "done";
  case TaskStatus::cancelled:
    return "cancelled";
  }
  return "todo";
}

std::optional<TaskStatus> parse_task_status(const std::string_view value) noexcept {
  if (value == "todo") {
    return TaskStatus::todo;
  }
  if (value == "in_progress") {
    return TaskStatus::in_progress;
  }
  if (value == "done") {
    return TaskStatus::done;
  }
  if (value == "cancelled") {
    return TaskStatus::cancelled;
  }
  return std::nullopt;
}

std::string_view task_priority_name(const TaskPriority priority) noexcept {
  switch (priority) {
  case TaskPriority::low:
    return "low";
  case TaskPriority::medium:
    return "medium";
  case TaskPriority::high:
    return "high";
  case TaskPriority::urgent:
    return "urgent";
  }
  return "medium";
}

std::optional<TaskPriority> parse_task_priority(const std::string_view value) noexcept {
  if (value == "low") {
    return TaskPriority::low;
  }
  if (value == "medium") {
    return TaskPriority::medium;
  }
  if (value == "high") {
    return TaskPriority::high;
  }
  if (value == "urgent") {
    return TaskPriority::urgent;
  }
  return std::nullopt;
}

} // namespace taskflow::domain
