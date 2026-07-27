#pragma once

#include "taskflow/application/tasks.hpp"

namespace taskflow::application {

enum class ReminderKind { pre_deadline, overdue };

struct ReminderRequest {
  ReminderKind kind;
  domain::Uuid task_id;
  std::uint64_t expected_version;
  domain::Uuid expected_assignee;
  domain::UtcInstant expected_deadline;
  std::string effect_key;
};

class ReminderEffectStore {
public:
  virtual ~ReminderEffectStore() = default;
  [[nodiscard]] virtual bool emit_once(std::string effect_key, const domain::Uuid &recipient_id,
                                       const domain::Task &task, ReminderKind kind) = 0;
};

class ReminderHandler {
public:
  ReminderHandler(TaskStore &tasks, ReminderEffectStore &effects, const domain::Clock &clock);
  [[nodiscard]] bool handle(const ReminderRequest &request) const;

private:
  TaskStore *tasks_;
  ReminderEffectStore *effects_;
  const domain::Clock *clock_;
};

} // namespace taskflow::application
