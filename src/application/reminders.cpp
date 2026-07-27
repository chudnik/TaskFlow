#include "taskflow/application/reminders.hpp"

namespace taskflow::application {

ReminderScheduler::ReminderScheduler(ReminderJobStore &jobs,
                                     const std::chrono::minutes pre_deadline_offset)
    : jobs_{&jobs}, pre_deadline_offset_{pre_deadline_offset} {}

void ReminderScheduler::task_changed(const domain::Task &task,
                                     const std::string_view correlation_id) const {
  const auto key = "task:" + task.id.to_string();
  jobs_->cancel_reminder(key + ":pre");
  jobs_->cancel_reminder(key + ":overdue");
  if (task.deleted() || !task.assignee_id || !task.deadline_at ||
      task.status == domain::TaskStatus::done || task.status == domain::TaskStatus::cancelled)
    return;
  const auto payload = "{\"task_id\":\"" + task.id.to_string() +
                       "\",\"version\":" + std::to_string(task.version) + ",\"assignee_id\":\"" +
                       task.assignee_id->to_string() + "\",\"deadline_at\":\"" +
                       domain::format_utc(*task.deadline_at) + "\"}";
  jobs_->upsert_reminder(key + ":pre", "task.pre_deadline", payload,
                         *task.deadline_at - pre_deadline_offset_, std::string{correlation_id});
  jobs_->upsert_reminder(key + ":overdue", "task.overdue", payload, *task.deadline_at,
                         std::string{correlation_id});
}

} // namespace taskflow::application
