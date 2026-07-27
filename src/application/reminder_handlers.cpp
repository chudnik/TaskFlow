#include "taskflow/application/reminder_handlers.hpp"

namespace taskflow::application {

ReminderHandler::ReminderHandler(TaskStore &tasks, ReminderEffectStore &effects,
                                 const domain::Clock &clock)
    : tasks_{&tasks}, effects_{&effects}, clock_{&clock} {}

bool ReminderHandler::handle(const ReminderRequest &request) const {
  const auto task = tasks_->find_active_task(request.task_id);
  if (!task || task->version != request.expected_version ||
      task->assignee_id != request.expected_assignee ||
      task->deadline_at != request.expected_deadline || task->status == domain::TaskStatus::done ||
      task->status == domain::TaskStatus::cancelled)
    return false;
  if (request.kind == ReminderKind::overdue && !task->overdue(clock_->now()))
    return false;
  if (request.kind == ReminderKind::pre_deadline && clock_->now() >= *task->deadline_at)
    return false;
  return effects_->emit_once(request.effect_key, *task->assignee_id, *task, request.kind);
}

} // namespace taskflow::application
