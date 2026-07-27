#pragma once

#include "taskflow/domain/task.hpp"

namespace taskflow::application {

class ReminderJobStore {
public:
  virtual ~ReminderJobStore() = default;
  virtual void upsert_reminder(std::string business_key, std::string type, std::string payload,
                               domain::UtcInstant scheduled_at, std::string correlation_id) = 0;
  virtual void cancel_reminder(std::string_view business_key) = 0;
};

class ReminderScheduler {
public:
  ReminderScheduler(ReminderJobStore &jobs, std::chrono::minutes pre_deadline_offset);
  void task_changed(const domain::Task &task, std::string_view correlation_id) const;

private:
  ReminderJobStore *jobs_;
  std::chrono::minutes pre_deadline_offset_;
};

} // namespace taskflow::application
