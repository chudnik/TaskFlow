#include "taskflow/domain/task.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow;

domain::UtcInstant instant(const std::string_view value) { return *domain::parse_utc(value); }

domain::Task task() {
  const auto at = instant("2026-07-27T10:00:00Z");
  return {domain::Uuid::generate(),
          domain::Uuid::generate(),
          "Implement tasks",
          "",
          domain::TaskStatus::todo,
          domain::TaskPriority::medium,
          domain::Uuid::generate(),
          {},
          instant("2026-07-27T11:00:00Z"),
          {},
          1,
          at,
          at,
          {},
          {}};
}

TEST(TaskModelTest, ValidatesFieldsAndParsesEnums) {
  EXPECT_TRUE(domain::validate_task_fields("Task", "").empty());
  EXPECT_FALSE(domain::validate_task_fields("", "").empty());
  EXPECT_EQ(domain::parse_task_status("in_progress"), domain::TaskStatus::in_progress);
  EXPECT_FALSE(domain::parse_task_status("unknown"));
  EXPECT_EQ(domain::parse_task_priority("urgent"), domain::TaskPriority::urgent);
}

TEST(TaskModelTest, EnforcesWorkflowAndCompletionTimestamp) {
  auto value = task();
  const auto started = instant("2026-07-27T10:10:00Z");
  EXPECT_FALSE(domain::apply_transition(value, domain::TaskStatus::done, started));
  EXPECT_TRUE(domain::apply_transition(value, domain::TaskStatus::in_progress, started));
  const auto completed = instant("2026-07-27T10:20:00Z");
  EXPECT_TRUE(domain::apply_transition(value, domain::TaskStatus::done, completed));
  EXPECT_EQ(value.completed_at, completed);
  EXPECT_TRUE(domain::apply_transition(value, domain::TaskStatus::todo, completed));
  EXPECT_FALSE(value.completed_at);
  EXPECT_EQ(value.version, 4U);
}

TEST(TaskModelTest, RequiresActiveMemberForAssignment) {
  auto value = task();
  const auto assignee = domain::Uuid::generate();
  EXPECT_FALSE(domain::assign(value, assignee, false, value.updated_at));
  EXPECT_FALSE(value.assignee_id);
  EXPECT_TRUE(domain::assign(value, assignee, true, value.updated_at));
  EXPECT_EQ(value.assignee_id, assignee);
  EXPECT_TRUE(domain::assign(value, std::nullopt, false, value.updated_at));
  EXPECT_FALSE(value.assignee_id);
}

TEST(TaskModelTest, CalculatesOverdueAndSoftDeletes) {
  auto value = task();
  EXPECT_FALSE(value.overdue(instant("2026-07-27T11:00:00Z")));
  EXPECT_TRUE(value.overdue(instant("2026-07-27T11:00:01Z")));
  const auto actor = domain::Uuid::generate();
  EXPECT_TRUE(domain::soft_delete(value, actor, instant("2026-07-27T12:00:00Z")));
  EXPECT_TRUE(value.deleted());
  EXPECT_EQ(value.deleted_by, actor);
  EXPECT_FALSE(value.overdue(instant("2026-07-27T13:00:00Z")));
  EXPECT_FALSE(domain::soft_delete(value, actor, instant("2026-07-27T13:00:00Z")));
}

} // namespace
