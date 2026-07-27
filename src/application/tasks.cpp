#include "taskflow/application/tasks.hpp"
#include "taskflow/application/reminders.hpp"

#include <utility>

namespace taskflow::application {

TaskError::TaskError(const TaskErrorCode code, std::string message,
                     std::optional<domain::Task> current)
    : std::runtime_error{std::move(message)}, code_{code}, current_{std::move(current)} {}
TaskErrorCode TaskError::code() const noexcept { return code_; }
const std::optional<domain::Task> &TaskError::current() const noexcept { return current_; }

TaskUseCases::TaskUseCases(ProjectStore &projects, TaskStore &tasks, const PolicyService &policy,
                           const domain::Clock &clock)
    : projects_{&projects}, tasks_{&tasks}, policy_{&policy}, clock_{&clock} {}

TaskUseCases::TaskUseCases(ProjectStore &projects, TaskStore &tasks, const PolicyService &policy,
                           const domain::Clock &clock, const ReminderScheduler &reminders)
    : projects_{&projects}, tasks_{&tasks}, policy_{&policy}, clock_{&clock},
      reminders_{&reminders} {}

void TaskUseCases::require_member(const AuthenticatedPrincipal &actor,
                                  const domain::Uuid &project_id) const {
  const auto project = projects_->find_visible_project(
      project_id, actor.user_id, actor.global_role == domain::GlobalRole::admin);
  if (!project) {
    throw TaskError{TaskErrorCode::not_found, "task not found"};
  }
  if (project->archived()) {
    throw TaskError{TaskErrorCode::archived, "archived project cannot be changed"};
  }
  if (actor.global_role == domain::GlobalRole::admin ||
      !projects_->find_role(project_id, actor.user_id)) {
    throw TaskError{TaskErrorCode::forbidden, "task action is forbidden"};
  }
}

domain::Task TaskUseCases::authorize(const AuthenticatedPrincipal &actor,
                                     const domain::Uuid &task_id, const TaskAction action) const {
  const auto task = tasks_->find_active_task(task_id);
  if (!task) {
    throw TaskError{TaskErrorCode::not_found, "task not found"};
  }
  const auto project = projects_->find_visible_project(
      task->project_id, actor.user_id, actor.global_role == domain::GlobalRole::admin);
  if (!project) {
    throw TaskError{TaskErrorCode::not_found, "task not found"};
  }
  const TaskAuthorizationContext context{
      actor.global_role, projects_->find_role(task->project_id, actor.user_id),
      task->creator_id == actor.user_id, task->assignee_id == actor.user_id};
  if (!policy_->permits(context, action)) {
    throw TaskError{TaskErrorCode::forbidden, "task action is forbidden"};
  }
  if (action != TaskAction::read && project->archived()) {
    throw TaskError{TaskErrorCode::archived, "archived project cannot be changed"};
  }
  return *task;
}

void TaskUseCases::require_current(const domain::Task &task, const std::uint64_t version) const {
  if (task.version != version) {
    throw TaskError{TaskErrorCode::conflict, "task version is stale", task};
  }
}

domain::Task TaskUseCases::create(const AuthenticatedPrincipal &actor,
                                  CreateTaskInput input) const {
  require_member(actor, input.project_id);
  const auto errors = domain::validate_task_fields(input.title, input.description);
  if (!errors.empty()) {
    throw TaskError{TaskErrorCode::invalid_input, errors.items().front().message};
  }
  if (input.assignee_id && !projects_->find_role(input.project_id, *input.assignee_id)) {
    throw TaskError{TaskErrorCode::invalid_assignee, "assignee must be an active project member"};
  }
  auto created =
      tasks_->create_task(input.project_id, std::move(input.title), std::move(input.description),
                          input.priority, actor.user_id, input.assignee_id, input.deadline_at);
  if (reminders_)
    reminders_->task_changed(created, "task-create");
  return created;
}

domain::Task TaskUseCases::read(const AuthenticatedPrincipal &actor,
                                const domain::Uuid &task_id) const {
  return authorize(actor, task_id, TaskAction::read);
}

domain::Task TaskUseCases::update(const AuthenticatedPrincipal &actor, const domain::Uuid &task_id,
                                  UpdateTaskInput input) const {
  auto task = authorize(actor, task_id, TaskAction::update);
  require_current(task, input.version);
  const auto errors = domain::validate_task_fields(input.title, input.description);
  if (!errors.empty()) {
    throw TaskError{TaskErrorCode::invalid_input, errors.items().front().message};
  }
  task.title = std::move(input.title);
  task.description = std::move(input.description);
  task.priority = input.priority;
  task.deadline_at = input.deadline_at;
  auto updated = tasks_->update_task(task, input.version);
  if (reminders_)
    reminders_->task_changed(updated, "task-update");
  return updated;
}

domain::Task TaskUseCases::transition(const AuthenticatedPrincipal &actor,
                                      const domain::Uuid &task_id, const domain::TaskStatus status,
                                      const std::uint64_t version) const {
  auto task = authorize(actor, task_id, TaskAction::change_status);
  require_current(task, version);
  if (!domain::apply_transition(task, status, clock_->now())) {
    throw TaskError{TaskErrorCode::invalid_transition, "task status transition is invalid"};
  }
  // The domain increment is prospective; PostgreSQL owns the actual increment.
  task.version = version;
  auto updated = tasks_->update_task(task, version);
  if (reminders_)
    reminders_->task_changed(updated, "task-transition");
  return updated;
}

domain::Task TaskUseCases::assign(const AuthenticatedPrincipal &actor, const domain::Uuid &task_id,
                                  const std::optional<domain::Uuid> assignee_id,
                                  const std::uint64_t version) const {
  auto task = authorize(actor, task_id, TaskAction::assign);
  require_current(task, version);
  const bool active = !assignee_id || projects_->find_role(task.project_id, *assignee_id);
  if (!active) {
    throw TaskError{TaskErrorCode::invalid_assignee, "assignee must be an active project member"};
  }
  static_cast<void>(domain::assign(task, assignee_id, true, clock_->now()));
  task.version = version;
  auto updated = tasks_->update_task(task, version);
  if (reminders_)
    reminders_->task_changed(updated, "task-assignment");
  return updated;
}

void TaskUseCases::remove(const AuthenticatedPrincipal &actor, const domain::Uuid &task_id,
                          const std::uint64_t version) const {
  const auto task = authorize(actor, task_id, TaskAction::remove);
  require_current(task, version);
  const auto deleted = tasks_->delete_task(task_id, version, actor.user_id);
  if (reminders_)
    reminders_->task_changed(deleted, "task-delete");
}

} // namespace taskflow::application
