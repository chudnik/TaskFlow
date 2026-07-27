#include "taskflow/application/reminder_handlers.hpp"

#include <gtest/gtest.h>
#include <set>

namespace {
using namespace taskflow;

class TaskState final : public application::TaskStore {
public:
  std::optional<domain::Task> task;
  domain::Task create_task(const domain::Uuid &, std::string, std::string, domain::TaskPriority,
                           const domain::Uuid &, std::optional<domain::Uuid>,
                           std::optional<domain::UtcInstant>) override {
    return *task;
  }
  std::optional<domain::Task> find_active_task(const domain::Uuid &) override { return task; }
  domain::Task update_task(const domain::Task &, std::uint64_t) override { return *task; }
  domain::Task delete_task(const domain::Uuid &, std::uint64_t, const domain::Uuid &) override {
    return *task;
  }
};

class Effects final : public application::ReminderEffectStore {
public:
  std::set<std::string> keys;
  bool emit_once(std::string key, const domain::Uuid &, const domain::Task &,
                 application::ReminderKind) override {
    return keys.insert(std::move(key)).second;
  }
};

TEST(ReminderHandlerTest, RevalidatesAndEmitsNoDuplicateEffect) {
  const auto now = *domain::parse_utc("2026-07-28T10:00:01Z");
  domain::FixedClock clock{now};
  const auto assignee = domain::Uuid::generate();
  const auto deadline = now - std::chrono::seconds{1};
  TaskState tasks;
  tasks.task = domain::Task{domain::Uuid::generate(),
                            domain::Uuid::generate(),
                            "Overdue",
                            "",
                            domain::TaskStatus::todo,
                            domain::TaskPriority::high,
                            domain::Uuid::generate(),
                            assignee,
                            deadline,
                            {},
                            4,
                            now - std::chrono::hours{1},
                            now - std::chrono::hours{1},
                            {},
                            {}};
  Effects effects;
  application::ReminderHandler handler{tasks, effects, clock};
  const application::ReminderRequest request{
      application::ReminderKind::overdue, tasks.task->id, 4, assignee, deadline, "overdue-effect"};
  EXPECT_TRUE(handler.handle(request));
  EXPECT_FALSE(handler.handle(request));
  tasks.task->version = 5;
  EXPECT_FALSE(handler.handle(request));
}

} // namespace
