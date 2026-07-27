#include "taskflow/application/task_query.hpp"
#include "taskflow/infrastructure/audit_repository.hpp"
#include "taskflow/infrastructure/job_repository.hpp"
#include "taskflow/infrastructure/notification_delivery.hpp"
#include "taskflow/infrastructure/outbox_repository.hpp"
#include "taskflow/infrastructure/project_repositories.hpp"
#include "taskflow/infrastructure/task_repositories.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

namespace {
using namespace taskflow;

std::string integration_dsn() {
  const char *value = std::getenv("TASKFLOW_TEST_POSTGRES_DSN");
  return value == nullptr ? std::string{} : std::string{value};
}

domain::Uuid insert_user(infrastructure::PostgresConnection &connection, const std::string &email) {
  const auto id = domain::Uuid::generate();
  static_cast<void>(connection.execute(
      "INSERT INTO users(id, email, password_hash) VALUES($1::uuid, $2, 'test-hash')",
      {id.to_string(), email}));
  return id;
}

TEST(ProjectRepositoriesIntegration, CreatesProjectAndOwnerAtomically) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto owner = insert_user(connection, "project-owner@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::ProjectMembershipRepository memberships{connection};

  const auto project = projects.create_with_owner("TaskFlow", "Project repository test", owner);
  const auto membership = memberships.find(project.id, owner);
  ASSERT_TRUE(membership);
  EXPECT_EQ(membership->role, domain::ProjectRole::owner);
  EXPECT_EQ(project.created_by, owner);
}

TEST(ProjectRepositoriesIntegration, RefusesToRemoveOrDemoteFinalOwner) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto first_owner = insert_user(connection, "first-owner@example.com");
  const auto second_owner = insert_user(connection, "second-owner@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::ProjectMembershipRepository memberships{connection};
  const auto project = projects.create_with_owner("Invariant", "", first_owner);

  EXPECT_THROW(memberships.remove(project.id, first_owner), infrastructure::RepositoryError);
  EXPECT_THROW(static_cast<void>(
                   memberships.change_role(project.id, first_owner, domain::ProjectRole::manager)),
               infrastructure::RepositoryError);

  static_cast<void>(memberships.add(project.id, second_owner, domain::ProjectRole::owner));
  EXPECT_NO_THROW(memberships.remove(project.id, first_owner));
  const auto remaining = memberships.list(project.id);
  ASSERT_EQ(remaining.size(), 1U);
  EXPECT_EQ(remaining.front().user_id, second_owner);
  EXPECT_EQ(remaining.front().role, domain::ProjectRole::owner);
}

TEST(ProjectRepositoriesIntegration, UpdatesArchivesAndListsVisibleProjects) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto owner = insert_user(connection, "lifecycle-owner@example.com");
  const auto outsider = insert_user(connection, "lifecycle-outsider@example.com");
  infrastructure::ProjectRepository projects{connection};
  const auto created = projects.create_project("Initial", "Description", owner);

  EXPECT_EQ(projects.list_projects(owner, false).size(), 1U);
  EXPECT_TRUE(projects.list_projects(outsider, false).empty());
  EXPECT_TRUE(projects.find_visible_project(created.id, owner, false));
  EXPECT_FALSE(projects.find_visible_project(created.id, outsider, false));
  EXPECT_TRUE(projects.find_visible_project(created.id, outsider, true));
  const auto updated = projects.update_project(created.id, "Updated", "New description");
  EXPECT_EQ(updated.name, "Updated");
  const auto archived = projects.archive_project(created.id, owner);
  EXPECT_TRUE(archived.archived());
  EXPECT_EQ(archived.archived_by, owner);
  EXPECT_THROW(static_cast<void>(projects.update_project(created.id, "Again", "Rejected")),
               infrastructure::RepositoryError);
}

TEST(ProjectRepositoriesIntegration, PersistsMembershipLifecycleForAllProjectRoles) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto owner = insert_user(connection, "role-owner@example.com");
  const auto manager = insert_user(connection, "role-manager@example.com");
  const auto member = insert_user(connection, "role-member@example.com");
  const auto outsider = insert_user(connection, "role-outsider@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::ProjectMembershipRepository memberships{connection};
  const auto project = projects.create_project("Roles", "", owner);

  EXPECT_EQ(memberships.add(project.id, manager, domain::ProjectRole::manager).role,
            domain::ProjectRole::manager);
  EXPECT_EQ(memberships.add(project.id, member, domain::ProjectRole::member).role,
            domain::ProjectRole::member);
  EXPECT_TRUE(projects.find_visible_project(project.id, owner, false));
  EXPECT_TRUE(projects.find_visible_project(project.id, manager, false));
  EXPECT_TRUE(projects.find_visible_project(project.id, member, false));
  EXPECT_FALSE(projects.find_visible_project(project.id, outsider, false));
  EXPECT_EQ(memberships.change_role(project.id, member, domain::ProjectRole::manager).role,
            domain::ProjectRole::manager);
  memberships.remove(project.id, member);
  EXPECT_FALSE(memberships.find(project.id, member));
}

TEST(ProjectRepositoriesIntegration, OptimisticallyUpdatesAndUnassignsTasksOnMemberRemoval) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto owner = insert_user(connection, "task-owner@example.com");
  const auto assignee = insert_user(connection, "task-assignee@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::ProjectMembershipRepository memberships{connection};
  infrastructure::TaskRepository tasks{connection};
  const auto project = projects.create_project("Tasks", "", owner);
  static_cast<void>(memberships.add(project.id, assignee, domain::ProjectRole::member));
  auto task = tasks.create(project.id, "Optimistic", "", domain::TaskPriority::high, owner,
                           assignee, std::nullopt);

  task.title = "Updated";
  const auto updated = tasks.update(task, 1);
  EXPECT_EQ(updated.version, 2U);
  EXPECT_THROW(static_cast<void>(tasks.update(task, 1)), infrastructure::RepositoryError);

  memberships.remove(project.id, assignee);
  const auto unassigned = tasks.find_active(task.id);
  ASSERT_TRUE(unassigned);
  EXPECT_FALSE(unassigned->assignee_id);
  EXPECT_EQ(unassigned->version, 3U);
  const auto deleted = tasks.soft_delete(task.id, 3, owner);
  EXPECT_TRUE(deleted.deleted());
  EXPECT_FALSE(tasks.find_active(task.id));
}

TEST(ProjectRepositoriesIntegration, FiltersSortsAndKeysetPaginatesAcrossConcurrentInsert) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto owner = insert_user(connection, "query-owner@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::TaskRepository tasks{connection};
  const auto project = projects.create_project("Query", "", owner);
  const auto deadline = domain::parse_utc("2026-07-28T10:00:00Z");
  static_cast<void>(tasks.create(project.id, "Alpha", "", domain::TaskPriority::high, owner,
                                 std::nullopt, deadline));
  static_cast<void>(tasks.create(project.id, "Beta", "", domain::TaskPriority::high, owner,
                                 std::nullopt, deadline));
  static_cast<void>(tasks.create(project.id, "Other", "", domain::TaskPriority::low, owner,
                                 std::nullopt, std::nullopt));
  const auto query = application::normalize_task_query(
      {project.id, {}, domain::TaskPriority::high, {}, owner, {}, {}, {}, {}},
      {application::TaskSortField::deadline, application::SortDirection::ascending});
  const auto first =
      tasks.list_tasks(query, owner, std::nullopt, 1, *domain::parse_utc("2026-07-27T10:00:00Z"));
  ASSERT_EQ(first.size(), 1U);
  static_cast<void>(tasks.create(project.id, "Concurrent", "", domain::TaskPriority::high, owner,
                                 std::nullopt, deadline));
  const application::TaskCursor after{1, application::task_query_fingerprint(query),
                                      domain::format_utc(*first.back().deadline_at),
                                      first.back().id};
  const auto second =
      tasks.list_tasks(query, owner, after, 10, *domain::parse_utc("2026-07-27T10:00:00Z"));
  EXPECT_FALSE(second.empty());
  EXPECT_TRUE(std::none_of(second.begin(), second.end(),
                           [&](const auto &task) { return task.id == first.front().id; }));
  EXPECT_TRUE(std::all_of(second.begin(), second.end(), [&](const auto &task) {
    return task.priority == domain::TaskPriority::high && task.creator_id == owner;
  }));
}

TEST(ProjectRepositoriesIntegration, CommitsMatchingAuditAndOutboxForEveryMutation) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto owner = insert_user(connection, "audit-owner@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::TaskRepository tasks{connection};
  const auto project = projects.create_project("Audited", "", owner);
  const auto deadline = *domain::parse_utc("2026-08-01T12:00:00Z");
  auto task = tasks.create(project.id, "Audited task", "", domain::TaskPriority::medium, owner,
                           owner, deadline);
  task.title = "Changed";
  static_cast<void>(tasks.update(task, 1));
  const auto counts = connection.execute("SELECT (SELECT count(*) FROM audit_events)::text, "
                                         "(SELECT count(*) FROM outbox_events)::text");
  EXPECT_EQ(*counts.value(0, 0), *counts.value(0, 1));
  EXPECT_EQ(*counts.value(0, 0), "4");
  const auto unmatched = connection.execute(
      "SELECT count(*)::text FROM audit_events a FULL JOIN outbox_events o "
      "ON o.event_id = a.event_id WHERE a.event_id IS NULL OR o.event_id IS NULL");
  EXPECT_EQ(*unmatched.value(0, 0), "0");
  const auto reminders = connection.execute(
      "SELECT count(*)::text FROM jobs WHERE business_key LIKE $1 AND status='pending'",
      {"task:" + task.id.to_string() + ":%"});
  EXPECT_EQ(*reminders.value(0, 0), "2");
}

TEST(ProjectRepositoriesIntegration, RollsBackAndClaimsOutboxIdempotentlyWithSanitizedAudit) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto owner = insert_user(connection, "outbox-owner@example.com");
  infrastructure::ProjectRepository projects{connection};
  const auto project = projects.create_project("Outbox", "", owner);
  const auto before = connection.execute("SELECT count(*)::text FROM outbox_events");
  {
    auto transaction = connection.transaction();
    static_cast<void>(transaction.execute(
        "UPDATE projects SET name = 'Rolled back' WHERE id = $1::uuid", {project.id.to_string()}));
    transaction.rollback();
  }
  const auto after = connection.execute("SELECT count(*)::text FROM outbox_events");
  EXPECT_EQ(*before.value(0, 0), *after.value(0, 0));

  infrastructure::OutboxRepository outbox{connection};
  const auto first = outbox.claim("publisher-a", 100, std::chrono::seconds{30});
  EXPECT_FALSE(first.empty());
  EXPECT_TRUE(outbox.claim("publisher-b", 100, std::chrono::seconds{30}).empty());
  outbox.mark_processed(first.front().event_id, "publisher-a");
  EXPECT_NO_THROW(outbox.mark_processed(first.front().event_id, "publisher-a"));

  infrastructure::AuditRepository audit{connection};
  const auto appended = audit.append_audit({0,
                                            domain::Uuid::generate(),
                                            project.id,
                                            {},
                                            owner,
                                            "security.review",
                                            "project",
                                            project.id,
                                            {},
                                            {{"access_token", "private"}, {"name", "visible"}},
                                            "audit-test",
                                            {}});
  EXPECT_EQ(appended.after.at("access_token"), "[REDACTED]");
  const auto history = audit.list_audit(project.id, {}, {}, 100);
  EXPECT_TRUE(std::any_of(history.items.begin(), history.items.end(), [&](const auto &event) {
    return event.event_id == appended.event_id && event.after.at("name") == "visible";
  }));
}

TEST(ProjectRepositoriesIntegration, MaterializesOrdersReplaysAndReauthorizesNotifications) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  const auto owner = insert_user(connection, "notify-owner@example.com");
  const auto member = insert_user(connection, "notify-member@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::ProjectMembershipRepository memberships{connection};
  infrastructure::OutboxRepository outbox{connection};
  infrastructure::NotificationRepository notifications{connection};
  infrastructure::NotificationDelivery delivery{notifications};
  const auto project = projects.create_project("Notify", "", owner);
  static_cast<void>(memberships.add(project.id, member, domain::ProjectRole::member));
  const auto events = outbox.claim("notifier", 100, std::chrono::seconds{30});
  ASSERT_FALSE(events.empty());
  for (const auto &event : events) {
    static_cast<void>(notifications.materialize(event, std::chrono::hours{24}));
    EXPECT_EQ(notifications.materialize(event, std::chrono::hours{24}), 0U);
  }
  const auto replay = delivery.resume(member, 0, 100);
  ASSERT_FALSE(replay.events.empty());
  EXPECT_TRUE(std::is_sorted(
      replay.events.begin(), replay.events.end(),
      [](const auto &left, const auto &right) { return left.sequence_id < right.sequence_id; }));
  EXPECT_NO_THROW(
      delivery.acknowledge(member, replay.events.front().sequence_id, replay.live_after_sequence));
  EXPECT_THROW(
      delivery.acknowledge(member, replay.live_after_sequence + 1, replay.live_after_sequence),
      infrastructure::RepositoryError);
  memberships.remove(project.id, member);
  EXPECT_TRUE(delivery.resume(member, 0, 100).events.empty());
}

TEST(ProjectRepositoriesIntegration, LeasesRetriesRecoversAndTerminatesJobsAcrossWorkers) {
  const auto dsn = integration_dsn();
  if (dsn.empty()) {
    GTEST_SKIP() << "TASKFLOW_TEST_POSTGRES_DSN is not configured";
  }
  infrastructure::PostgresConnection connection{dsn};
  static_cast<void>(connection.execute("SET taskflow.test_database = 'on'"));
  infrastructure::reset_database_for_tests(connection);
  infrastructure::JobRepository jobs{connection};
  const auto now = *domain::parse_utc("2026-07-01T00:00:00Z");
  const auto id = jobs.schedule("unique-job", "test", "{}", now, 2, "job-correlation");
  EXPECT_EQ(jobs.schedule("unique-job", "test", "{\"new\":true}", now, 2, "job-correlation"), id);
  const auto first = jobs.lease_due("worker-a", 1, std::chrono::seconds{30});
  ASSERT_EQ(first.size(), 1U);
  EXPECT_TRUE(jobs.lease_due("worker-b", 1, std::chrono::seconds{30}).empty());
  jobs.fail(id, "worker-a", "transient", std::chrono::seconds{0});
  const auto retry = jobs.lease_due("worker-b", 1, std::chrono::seconds{30});
  ASSERT_EQ(retry.size(), 1U);
  EXPECT_EQ(retry.front().attempt, 2U);
  jobs.fail(id, "worker-b", "terminal", std::chrono::seconds{0});
  EXPECT_TRUE(jobs.lease_due("worker-a", 1, std::chrono::seconds{30}).empty());
  const auto state =
      connection.execute("SELECT status,last_error FROM jobs WHERE id=$1::uuid", {id.to_string()});
  EXPECT_EQ(*state.value(0, 0), "failed");
  EXPECT_EQ(*state.value(0, 1), "terminal");

  const auto recoverable = jobs.schedule("expired-lease", "test", "{}", now, 3, "lease-recovery");
  ASSERT_EQ(jobs.lease_due("worker-a", 1, std::chrono::seconds{30}).size(), 1U);
  static_cast<void>(
      connection.execute("UPDATE jobs SET lease_expires_at=clock_timestamp()-interval '1 second' "
                         "WHERE id=$1::uuid",
                         {recoverable.to_string()}));
  const auto recovered = jobs.lease_due("worker-b", 1, std::chrono::seconds{30});
  ASSERT_EQ(recovered.size(), 1U);
  EXPECT_EQ(recovered.front().id, recoverable);
  jobs.succeed(recoverable, "worker-b");

  const auto owner = insert_user(connection, "reminder-owner@example.com");
  infrastructure::ProjectRepository projects{connection};
  infrastructure::TaskRepository tasks{connection};
  const auto project = projects.create_project("Reminder supersession", "", owner);
  const auto first_deadline = *domain::parse_utc("2030-01-01T12:00:00Z");
  auto task = tasks.create(project.id, "Deadline", "", domain::TaskPriority::medium, owner, owner,
                           first_deadline);
  const auto second_deadline = *domain::parse_utc("2030-01-02T12:00:00Z");
  task.deadline_at = second_deadline;
  static_cast<void>(tasks.update(task, 1));
  const auto reminder_state =
      connection.execute("SELECT count(*)::text, min((payload->>'version')::int)::text, "
                         "count(DISTINCT business_key)::text FROM jobs "
                         "WHERE business_key LIKE $1 AND status='pending'",
                         {"task:" + task.id.to_string() + ":%"});
  EXPECT_EQ(*reminder_state.value(0, 0), "2");
  EXPECT_EQ(*reminder_state.value(0, 1), "2");
  EXPECT_EQ(*reminder_state.value(0, 2), "2");
}

} // namespace
