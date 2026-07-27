#include "taskflow/application/comments.hpp"
#include "taskflow/application/tasks.hpp"
#include "taskflow/infrastructure/task_cursor.hpp"
#include "taskflow/transport/http/comment_controller.hpp"
#include "taskflow/transport/http/task_controller.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>

namespace {
using namespace taskflow;

domain::UtcInstant now() { return *domain::parse_utc("2026-07-27T10:00:00Z"); }

application::AuthenticatedPrincipal principal(const domain::Uuid &id) {
  return {id, domain::Uuid::generate(), domain::GlobalRole::user, now() + std::chrono::hours{1}};
}

class Projects final : public application::ProjectStore {
public:
  domain::Project project{
      domain::Uuid::generate(), "Tasks", "", domain::Uuid::generate(), now(), now(), {}, {}};
  std::vector<domain::ProjectMembership> members;

  domain::Project create_project(std::string, std::string, const domain::Uuid &) override {
    return project;
  }
  std::optional<domain::Project> find_project(const domain::Uuid &id) override {
    return id == project.id ? std::optional{project} : std::nullopt;
  }
  std::optional<domain::Project>
  find_visible_project(const domain::Uuid &id, const domain::Uuid &user, const bool all) override {
    return id == project.id && (all || find_role(id, user)) ? std::optional{project} : std::nullopt;
  }
  std::optional<domain::ProjectRole> find_role(const domain::Uuid &project_id,
                                               const domain::Uuid &user_id) override {
    const auto found = std::find_if(members.begin(), members.end(), [&](const auto &value) {
      return value.project_id == project_id && value.user_id == user_id;
    });
    return found == members.end() ? std::nullopt : std::optional{found->role};
  }
  domain::Project update_project(const domain::Uuid &, std::string, std::string) override {
    return project;
  }
  domain::Project archive_project(const domain::Uuid &, const domain::Uuid &) override {
    return project;
  }
  std::vector<domain::Project> list_projects(const domain::Uuid &, bool) override {
    return {project};
  }
};

class Tasks final : public application::TaskStore, public application::TaskQueryStore {
public:
  std::vector<domain::Task> values;

  domain::Task create_task(const domain::Uuid &project_id, std::string title,
                           std::string description, const domain::TaskPriority priority,
                           const domain::Uuid &creator, const std::optional<domain::Uuid> assignee,
                           const std::optional<domain::UtcInstant> deadline) override {
    values.push_back({domain::Uuid::generate(),
                      project_id,
                      std::move(title),
                      std::move(description),
                      domain::TaskStatus::todo,
                      priority,
                      creator,
                      assignee,
                      deadline,
                      {},
                      1,
                      now(),
                      now(),
                      {},
                      {}});
    return values.back();
  }
  std::optional<domain::Task> find_active_task(const domain::Uuid &id) override {
    const auto found = std::find_if(values.begin(), values.end(), [&](const auto &value) {
      return value.id == id && !value.deleted();
    });
    return found == values.end() ? std::nullopt : std::optional{*found};
  }
  domain::Task update_task(const domain::Task &task, const std::uint64_t expected) override {
    auto &stored = *std::find_if(values.begin(), values.end(),
                                 [&](const auto &value) { return value.id == task.id; });
    if (stored.version != expected) {
      throw application::TaskError{application::TaskErrorCode::conflict, "task version is stale",
                                   stored};
    }
    stored = task;
    stored.version = expected + 1;
    return stored;
  }
  domain::Task delete_task(const domain::Uuid &id, const std::uint64_t expected,
                           const domain::Uuid &actor) override {
    auto &stored = *std::find_if(values.begin(), values.end(),
                                 [&](const auto &value) { return value.id == id; });
    if (stored.version != expected) {
      throw application::TaskError{application::TaskErrorCode::conflict, "task version is stale",
                                   stored};
    }
    stored.deleted_at = now();
    stored.deleted_by = actor;
    ++stored.version;
    return stored;
  }
  std::vector<domain::Task> list_tasks(const application::NormalizedTaskQuery &query,
                                       const domain::Uuid &,
                                       const std::optional<application::TaskCursor> after,
                                       const std::size_t limit, domain::UtcInstant) override {
    std::vector<domain::Task> result;
    for (const auto &value : values) {
      if (value.project_id != query.filters.project_id || value.deleted())
        continue;
      if (after && !(value.id > after->task_id))
        continue;
      result.push_back(value);
    }
    std::sort(result.begin(), result.end(),
              [](const auto &left, const auto &right) { return left.id < right.id; });
    if (result.size() > limit)
      result.erase(result.begin() + static_cast<std::ptrdiff_t>(limit), result.end());
    return result;
  }
};

class Comments final : public application::CommentStore {
public:
  std::vector<domain::Comment> values;
  domain::Comment create_comment(const domain::Uuid &task_id, const domain::Uuid &author_id,
                                 std::string body) override {
    values.push_back(
        {domain::Uuid::generate(), task_id, author_id, std::move(body), now(), now(), {}, {}});
    return values.back();
  }
  std::optional<domain::Comment> find_comment(const domain::Uuid &id) override {
    const auto found = std::find_if(values.begin(), values.end(), [&](const auto &value) {
      return value.id == id && !value.deleted();
    });
    return found == values.end() ? std::nullopt : std::optional{*found};
  }
  std::vector<domain::Comment> list_comments(const domain::Uuid &task_id) override {
    std::vector<domain::Comment> result;
    std::copy_if(values.begin(), values.end(), std::back_inserter(result),
                 [&](const auto &value) { return value.task_id == task_id && !value.deleted(); });
    return result;
  }
  domain::Comment update_comment(const domain::Uuid &id, std::string body) override {
    auto &value = *std::find_if(values.begin(), values.end(),
                                [&](const auto &comment) { return comment.id == id; });
    value.body = std::move(body);
    return value;
  }
  void delete_comment(const domain::Uuid &id, const domain::Uuid &actor) override {
    auto &value = *std::find_if(values.begin(), values.end(),
                                [&](const auto &comment) { return comment.id == id; });
    value.deleted_at = now();
    value.deleted_by = actor;
  }
};

struct Fixture {
  Projects projects;
  Tasks tasks;
  application::PolicyService policy;
  domain::FixedClock clock{now()};
  domain::Uuid owner = domain::Uuid::generate();
  domain::Uuid manager = domain::Uuid::generate();
  domain::Uuid creator = domain::Uuid::generate();
  domain::Uuid assignee = domain::Uuid::generate();
  domain::Uuid member = domain::Uuid::generate();

  Fixture() {
    projects.members = {{projects.project.id, owner, domain::ProjectRole::owner, now(), now()},
                        {projects.project.id, manager, domain::ProjectRole::manager, now(), now()},
                        {projects.project.id, creator, domain::ProjectRole::member, now(), now()},
                        {projects.project.id, assignee, domain::ProjectRole::member, now(), now()},
                        {projects.project.id, member, domain::ProjectRole::member, now(), now()}};
  }
};

TEST(TaskUseCasesTest, ImplementsAuthorizedLifecycleAndStaleResponse) {
  Fixture fixture;
  application::TaskUseCases use_cases{fixture.projects, fixture.tasks, fixture.policy,
                                      fixture.clock};
  transport::http::TaskController controller{use_cases};
  const auto created =
      controller.create(principal(fixture.creator),
                        nlohmann::json{{"project_id", fixture.projects.project.id.to_string()},
                                       {"title", "Ship task"},
                                       {"assignee_id", fixture.assignee.to_string()},
                                       {"deadline_at", "2026-07-28T10:00:00Z"}}
                            .dump());
  ASSERT_EQ(created.status, 201);
  const auto id = nlohmann::json::parse(created.body)["id"].get<std::string>();

  EXPECT_EQ(controller
                .update(principal(fixture.creator), id,
                        R"({"title":"Updated","description":"","priority":"high","version":1})")
                .status,
            200);
  EXPECT_EQ(
      controller
          .transition(principal(fixture.assignee), id, R"({"status":"in_progress","version":2})")
          .status,
      200);
  EXPECT_EQ(controller.assign(principal(fixture.creator), id, R"({"assignee_id":null,"version":3})")
                .status,
            200);
  const auto stale =
      controller.update(principal(fixture.creator), id,
                        R"({"title":"Stale","description":"","priority":"low","version":1})");
  EXPECT_EQ(stale.status, 409);
  EXPECT_EQ(nlohmann::json::parse(stale.body)["error"]["details"][0]["current"]["version"], 4);
  EXPECT_EQ(controller.remove(principal(fixture.manager), id, R"({"version":4})").status, 204);
  EXPECT_EQ(controller.read(principal(fixture.creator), id).status, 404);
}

TEST(TaskUseCasesTest, RejectsUnauthorizedAndInvalidAssignmentsAndTransitions) {
  Fixture fixture;
  application::TaskUseCases use_cases{fixture.projects, fixture.tasks, fixture.policy,
                                      fixture.clock};
  const auto task = use_cases.create(
      principal(fixture.creator),
      {fixture.projects.project.id, "Task", "", domain::TaskPriority::medium, {}, {}});

  EXPECT_THROW(static_cast<void>(use_cases.update(principal(fixture.member), task.id,
                                                  {"No", "", domain::TaskPriority::low, {}, 1})),
               application::TaskError);
  EXPECT_THROW(static_cast<void>(use_cases.assign(principal(fixture.creator), task.id,
                                                  domain::Uuid::generate(), 1)),
               application::TaskError);
  EXPECT_THROW(static_cast<void>(use_cases.transition(principal(fixture.creator), task.id,
                                                      domain::TaskStatus::done, 1)),
               application::TaskError);
  EXPECT_THROW(use_cases.remove(principal(fixture.creator), task.id, 1), application::TaskError);
}

TEST(TaskUseCasesTest, ListsAuthorizedPagesWithBoundedOpaqueCursor) {
  Fixture fixture;
  application::TaskUseCases use_cases{fixture.projects, fixture.tasks, fixture.policy,
                                      fixture.clock};
  infrastructure::SignedTaskCursorCodec codec{"test-task-list-signing-secret-at-least-32-bytes"};
  application::TaskListUseCase list_use_case{fixture.projects, fixture.tasks, codec, fixture.clock};
  transport::http::TaskController controller{use_cases, list_use_case};
  for (int index = 0; index < 3; ++index)
    static_cast<void>(use_cases.create(principal(fixture.creator), {fixture.projects.project.id,
                                                                    "Task " + std::to_string(index),
                                                                    "",
                                                                    domain::TaskPriority::medium,
                                                                    {},
                                                                    {}}));
  const auto query = application::normalize_task_query(
      {fixture.projects.project.id, {}, {}, {}, {}, {}, {}, {}, {}},
      {application::TaskSortField::created_at, application::SortDirection::ascending});
  const auto first =
      controller.list(principal(fixture.creator), query, *domain::PageRequest::create(2));
  ASSERT_EQ(first.status, 200);
  const auto first_body = nlohmann::json::parse(first.body);
  EXPECT_EQ(first_body["items"].size(), 2U);
  ASSERT_TRUE(first_body["next_cursor"].is_string());
  const auto second = controller.list(
      principal(fixture.creator), query,
      *domain::PageRequest::create(2, first_body["next_cursor"].get<std::string>()));
  EXPECT_EQ(nlohmann::json::parse(second.body)["items"].size(), 1U);
  EXPECT_EQ(
      controller.list(principal(domain::Uuid::generate()), query, *domain::PageRequest::create(100))
          .status,
      404);
  EXPECT_FALSE(domain::PageRequest::create(101));

  const auto mismatched = application::normalize_task_query(
      {fixture.projects.project.id, domain::TaskStatus::todo, {}, {}, {}, {}, {}, {}, {}},
      {application::TaskSortField::created_at, application::SortDirection::ascending});
  EXPECT_EQ(controller
                .list(principal(fixture.creator), mismatched,
                      *domain::PageRequest::create(2, first_body["next_cursor"].get<std::string>()))
                .status,
            400);
}

TEST(TaskUseCasesTest, EnforcesCommentAuthorAndModeratorContract) {
  Fixture fixture;
  Comments comments;
  application::TaskUseCases task_use_cases{fixture.projects, fixture.tasks, fixture.policy,
                                           fixture.clock};
  const auto task = task_use_cases.create(
      principal(fixture.creator),
      {fixture.projects.project.id, "Comments", "", domain::TaskPriority::medium, {}, {}});
  application::CommentUseCases use_cases{fixture.projects, fixture.tasks, comments};
  transport::http::CommentController controller{use_cases};
  const auto created =
      controller.create(principal(fixture.creator), task.id.to_string(), R"({"body":"Hello"})");
  ASSERT_EQ(created.status, 201);
  const auto id = nlohmann::json::parse(created.body)["id"].get<std::string>();
  EXPECT_EQ(controller.edit(principal(fixture.member), id, R"({"body":"Forbidden"})").status, 403);
  EXPECT_EQ(controller.edit(principal(fixture.manager), id, R"({"body":"Moderated"})").status, 200);
  EXPECT_EQ(controller.list(principal(fixture.member), task.id.to_string()).status, 200);
  EXPECT_EQ(controller.remove(principal(fixture.creator), id).status, 204);
  EXPECT_TRUE(comments.values.front().deleted());
  EXPECT_EQ(controller.create(principal(fixture.creator), task.id.to_string(), R"({"body":"   "})")
                .status,
            422);
  EXPECT_TRUE(domain::validate_comment_body(std::string(10'001, 'x')).items().size() == 1);
}

} // namespace
