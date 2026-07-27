#include "taskflow/application/reminders.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow;

class ReminderJobs final : public application::ReminderJobStore {
public:
  struct Scheduled {
    std::string key;
    std::string type;
    domain::UtcInstant at;
  };
  std::vector<Scheduled> scheduled;
  std::vector<std::string> cancelled;
  void upsert_reminder(std::string key, std::string type, std::string, domain::UtcInstant at,
                       std::string) override {
    scheduled.push_back({std::move(key), std::move(type), at});
  }
  void cancel_reminder(const std::string_view key) override { cancelled.emplace_back(key); }
};

TEST(ReminderSchedulerTest, ReschedulesCurrentAssignedDeadlineAndCancelsObsoleteJobs) {
  ReminderJobs jobs;
  application::ReminderScheduler scheduler{jobs, std::chrono::minutes{30}};
  const auto deadline = *domain::parse_utc("2026-07-28T10:00:00Z");
  domain::Task task{domain::Uuid::generate(),
                    domain::Uuid::generate(),
                    "Reminder",
                    "",
                    domain::TaskStatus::todo,
                    domain::TaskPriority::medium,
                    domain::Uuid::generate(),
                    domain::Uuid::generate(),
                    deadline,
                    {},
                    3,
                    deadline - std::chrono::hours{1},
                    deadline - std::chrono::hours{1},
                    {},
                    {}};
  scheduler.task_changed(task, "corr");
  EXPECT_EQ(jobs.cancelled.size(), 2U);
  ASSERT_EQ(jobs.scheduled.size(), 2U);
  EXPECT_EQ(jobs.scheduled.front().at, deadline - std::chrono::minutes{30});
  task.assignee_id.reset();
  scheduler.task_changed(task, "corr");
  EXPECT_EQ(jobs.scheduled.size(), 2U);
}

} // namespace
