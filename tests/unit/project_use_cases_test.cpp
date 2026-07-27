#include "taskflow/application/projects.hpp"
#include "taskflow/transport/http/project_controller.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>

namespace {
using namespace taskflow;

domain::UtcInstant now() { return *domain::parse_utc("2026-07-21T10:00:00Z"); }

application::AuthenticatedPrincipal
principal(const domain::Uuid &user_id, const domain::GlobalRole role = domain::GlobalRole::user) {
  return {user_id, domain::Uuid::generate(), role, now() + std::chrono::hours{1}};
}

class FakeProjectStore final : public application::ProjectStore {
public:
  std::vector<domain::Project> projects;
  std::vector<domain::ProjectMembership> memberships;

  domain::Project create_project(std::string name, std::string description,
                                 const domain::Uuid &owner_id) override {
    const auto project_id = domain::Uuid::generate();
    projects.push_back(
        {project_id, std::move(name), std::move(description), owner_id, now(), now(), {}, {}});
    memberships.push_back({project_id, owner_id, domain::ProjectRole::owner, now(), now()});
    return projects.back();
  }
  std::optional<domain::Project> find_project(const domain::Uuid &project_id) override {
    const auto found = std::find_if(projects.begin(), projects.end(),
                                    [&](const auto &value) { return value.id == project_id; });
    return found == projects.end() ? std::nullopt : std::optional{*found};
  }
  std::optional<domain::Project> find_visible_project(const domain::Uuid &project_id,
                                                      const domain::Uuid &user_id,
                                                      const bool include_all) override {
    const auto project = find_project(project_id);
    return project && (include_all || find_role(project_id, user_id)) ? project : std::nullopt;
  }
  std::optional<domain::ProjectRole> find_role(const domain::Uuid &project_id,
                                               const domain::Uuid &user_id) override {
    const auto found = std::find_if(memberships.begin(), memberships.end(), [&](const auto &value) {
      return value.project_id == project_id && value.user_id == user_id;
    });
    return found == memberships.end() ? std::nullopt : std::optional{found->role};
  }
  domain::Project update_project(const domain::Uuid &project_id, std::string name,
                                 std::string description) override {
    auto &project = *std::find_if(projects.begin(), projects.end(),
                                  [&](const auto &value) { return value.id == project_id; });
    project.name = std::move(name);
    project.description = std::move(description);
    return project;
  }
  domain::Project archive_project(const domain::Uuid &project_id,
                                  const domain::Uuid &actor_id) override {
    auto &project = *std::find_if(projects.begin(), projects.end(),
                                  [&](const auto &value) { return value.id == project_id; });
    project.archived_at = now();
    project.archived_by = actor_id;
    return project;
  }
  std::vector<domain::Project> list_projects(const domain::Uuid &user_id,
                                             const bool include_all) override {
    if (include_all) {
      return projects;
    }
    std::vector<domain::Project> visible;
    for (const auto &project : projects) {
      if (find_role(project.id, user_id)) {
        visible.push_back(project);
      }
    }
    return visible;
  }
};

TEST(ProjectUseCasesTest, CreatesProjectAndListsOnlyVisibleProjects) {
  FakeProjectStore store;
  application::PolicyService policy;
  application::ProjectUseCases use_cases{store, policy};
  const auto owner_id = domain::Uuid::generate();
  const auto outsider_id = domain::Uuid::generate();
  const transport::http::ProjectController controller{use_cases};

  const auto created = controller.create(principal(owner_id), R"({"name":"TaskFlow"})");
  EXPECT_EQ(created.status, 201);
  EXPECT_EQ(nlohmann::json::parse(created.body)["name"], "TaskFlow");
  EXPECT_EQ(nlohmann::json::parse(controller.list(principal(owner_id)).body)["items"].size(), 1U);
  EXPECT_TRUE(nlohmann::json::parse(controller.list(principal(outsider_id)).body)["items"].empty());
}

TEST(ProjectUseCasesTest, EnforcesReadUpdateAndArchivePolicies) {
  FakeProjectStore store;
  application::PolicyService policy;
  application::ProjectUseCases use_cases{store, policy};
  const auto owner_id = domain::Uuid::generate();
  const auto member_id = domain::Uuid::generate();
  const auto project = use_cases.create(principal(owner_id), "TaskFlow", "Initial");
  store.memberships.push_back({project.id, member_id, domain::ProjectRole::member, now(), now()});
  const transport::http::ProjectController controller{use_cases};

  const auto hidden = controller.read(principal(domain::Uuid::generate()), project.id.to_string());
  const auto missing =
      controller.read(principal(domain::Uuid::generate()), domain::Uuid::generate().to_string());
  EXPECT_EQ(hidden.status, 404);
  EXPECT_EQ(hidden.body, missing.body);
  EXPECT_EQ(controller
                .update(principal(member_id), project.id.to_string(),
                        R"({"name":"Changed","description":"No"})")
                .status,
            403);
  EXPECT_EQ(controller
                .update(principal(owner_id), project.id.to_string(),
                        R"({"name":"Changed","description":"Yes"})")
                .status,
            200);
  EXPECT_EQ(controller.archive(principal(member_id), project.id.to_string()).status, 403);
  EXPECT_EQ(controller.archive(principal(owner_id), project.id.to_string()).status, 200);
  EXPECT_EQ(controller
                .update(principal(owner_id), project.id.to_string(),
                        R"({"name":"Again","description":"No"})")
                .status,
            409);
}

} // namespace
