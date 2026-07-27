#include "taskflow/application/identity.hpp"
#include "taskflow/infrastructure/comment_repositories.hpp"
#include "taskflow/infrastructure/identity_repositories.hpp"
#include "taskflow/infrastructure/job_repository.hpp"
#include "taskflow/infrastructure/notification_delivery.hpp"
#include "taskflow/infrastructure/outbox_repository.hpp"
#include "taskflow/infrastructure/password_hasher.hpp"
#include "taskflow/infrastructure/project_repositories.hpp"
#include "taskflow/infrastructure/task_repositories.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

namespace {
using namespace taskflow;

TEST(DockerComposeEndToEndSmoke, CoversFirstReleaseLifecycle) {
  const char *dsn_value = std::getenv("TASKFLOW_TEST_POSTGRES_DSN");
  if (dsn_value == nullptr)
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";

  infrastructure::PostgresConnection connection{dsn_value};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);

  infrastructure::UserRepository users{connection};
  const infrastructure::PasswordHasher passwords{
      {.time_cost = 1, .memory_cost_kib = 8192, .parallelism = 1}};
  const application::IdentityUseCases identity{users, passwords};
  const auto owner = identity.register_user("owner@e2e.test", "correct horse battery");
  const auto member = identity.register_user("member@e2e.test", "correct horse battery");
  EXPECT_EQ(identity.login("OWNER@e2e.test", "correct horse battery").id, owner.id);

  infrastructure::ProjectRepository projects{connection};
  infrastructure::ProjectMembershipRepository memberships{connection};
  const auto project = projects.create_project("E2E project", "Compose smoke", owner.id);
  const auto membership = memberships.add(project.id, member.id, domain::ProjectRole::member);
  EXPECT_EQ(membership.role, domain::ProjectRole::member);

  infrastructure::TaskRepository tasks{connection};
  const auto deadline = *domain::parse_utc("2026-07-28T23:00:00Z");
  auto task = tasks.create(project.id, "Release task", "Verify lifecycle",
                           domain::TaskPriority::high, owner.id, member.id, deadline);
  task.status = domain::TaskStatus::in_progress;
  task = tasks.update(task, task.version);
  EXPECT_EQ(task.status, domain::TaskStatus::in_progress);

  infrastructure::CommentRepository comments{connection};
  auto comment = comments.create_comment(task.id, member.id, "Initial comment");
  comment = comments.update_comment(comment.id, "Edited comment");
  EXPECT_EQ(comment.body, "Edited comment");
  comments.delete_comment(comment.id, member.id);
  EXPECT_TRUE(comments.list_comments(task.id).empty());

  infrastructure::OutboxRepository outbox{connection};
  infrastructure::NotificationRepository notifications{connection};
  infrastructure::NotificationDelivery delivery{notifications};
  const auto events = outbox.claim("e2e-publisher", 100, std::chrono::seconds{30});
  ASSERT_FALSE(events.empty());
  for (const auto &event : events)
    static_cast<void>(notifications.materialize(event, std::chrono::hours{24}));
  const auto replay = delivery.resume(member.id, 0, 100);
  ASSERT_FALSE(replay.events.empty());
  EXPECT_NO_THROW(delivery.acknowledge(member.id, replay.events.back().sequence_id,
                                       replay.live_after_sequence));

  infrastructure::JobRepository jobs{connection};
  const auto job_id =
      jobs.schedule("task:" + task.id.to_string() + ":overdue:" + std::to_string(task.version),
                    "task.overdue", "{}", deadline, 3, "e2e-deadline");
  static_cast<void>(
      connection.execute("UPDATE jobs SET scheduled_at = clock_timestamp() WHERE id = $1::uuid",
                         {job_id.to_string()}));
  const auto leased = jobs.lease_due("e2e-worker", 10, std::chrono::seconds{30});
  ASSERT_EQ(leased.size(), 1U);
  jobs.succeed(leased.front().id, "e2e-worker");
  const auto status =
      connection.execute("SELECT status FROM jobs WHERE id = $1::uuid", {job_id.to_string()});
  ASSERT_EQ(status.row_count(), 1U);
  EXPECT_EQ(*status.value(0, 0), "succeeded");
}

} // namespace
