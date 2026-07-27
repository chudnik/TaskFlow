#include "taskflow/application/projects.hpp"
#include "taskflow/transport/http/membership_controller.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>

namespace {
using namespace taskflow;

domain::UtcInstant now() { return *domain::parse_utc("2026-07-27T10:00:00Z"); }

application::AuthenticatedPrincipal
principal(const domain::Uuid &user_id,
          const domain::GlobalRole global_role = domain::GlobalRole::user) {
  return {user_id, domain::Uuid::generate(), global_role, now() + std::chrono::hours{1}};
}

class FakeProjectStore final : public application::ProjectStore {
public:
  domain::Project project{
      domain::Uuid::generate(), "TaskFlow", "", domain::Uuid::generate(), now(), now(), {}, {}};
  std::vector<domain::ProjectMembership> *memberships{};

  domain::Project create_project(std::string, std::string, const domain::Uuid &) override {
    return project;
  }
  std::optional<domain::Project> find_project(const domain::Uuid &id) override {
    return id == project.id ? std::optional{project} : std::nullopt;
  }
  std::optional<domain::Project> find_visible_project(const domain::Uuid &id,
                                                      const domain::Uuid &user_id,
                                                      const bool include_all) override {
    const auto found = find_project(id);
    return found && (include_all || find_role(id, user_id)) ? found : std::nullopt;
  }
  std::optional<domain::ProjectRole> find_role(const domain::Uuid &project_id,
                                               const domain::Uuid &user_id) override {
    const auto found =
        std::find_if(memberships->begin(), memberships->end(), [&](const auto &membership) {
          return membership.project_id == project_id && membership.user_id == user_id;
        });
    return found == memberships->end() ? std::nullopt : std::optional{found->role};
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

class FakeMembershipStore final : public application::MembershipStore {
public:
  std::vector<domain::ProjectMembership> memberships;

  std::optional<domain::ProjectMembership> find_membership(const domain::Uuid &project_id,
                                                           const domain::Uuid &user_id) override {
    const auto found =
        std::find_if(memberships.begin(), memberships.end(), [&](const auto &membership) {
          return membership.project_id == project_id && membership.user_id == user_id;
        });
    return found == memberships.end() ? std::nullopt : std::optional{*found};
  }
  std::vector<domain::ProjectMembership> list_memberships(const domain::Uuid &project_id) override {
    std::vector<domain::ProjectMembership> result;
    std::copy_if(memberships.begin(), memberships.end(), std::back_inserter(result),
                 [&](const auto &membership) { return membership.project_id == project_id; });
    return result;
  }
  domain::ProjectMembership add_membership(const domain::Uuid &project_id,
                                           const domain::Uuid &user_id,
                                           const domain::ProjectRole role) override {
    memberships.push_back({project_id, user_id, role, now(), now()});
    return memberships.back();
  }
  domain::ProjectMembership change_membership_role(const domain::Uuid &project_id,
                                                   const domain::Uuid &user_id,
                                                   const domain::ProjectRole role) override {
    auto &membership =
        *std::find_if(memberships.begin(), memberships.end(), [&](const auto &candidate) {
          return candidate.project_id == project_id && candidate.user_id == user_id;
        });
    membership.role = role;
    membership.updated_at = now();
    return membership;
  }
  void remove_membership(const domain::Uuid &project_id, const domain::Uuid &user_id) override {
    memberships.erase(std::remove_if(memberships.begin(), memberships.end(),
                                     [&](const auto &membership) {
                                       return membership.project_id == project_id &&
                                              membership.user_id == user_id;
                                     }),
                      memberships.end());
  }
};

struct MembershipFixture {
  FakeMembershipStore memberships;
  FakeProjectStore projects;
  application::PolicyService policy;
  domain::Uuid owner = domain::Uuid::generate();
  domain::Uuid manager = domain::Uuid::generate();
  domain::Uuid member = domain::Uuid::generate();

  MembershipFixture() {
    projects.memberships = &memberships.memberships;
    memberships.memberships = {
        {projects.project.id, owner, domain::ProjectRole::owner, now(), now()},
        {projects.project.id, manager, domain::ProjectRole::manager, now(), now()},
        {projects.project.id, member, domain::ProjectRole::member, now(), now()}};
  }
};

TEST(MembershipUseCasesTest, OwnerAddsChangesListsAndRemovesMemberships) {
  MembershipFixture fixture;
  application::MembershipUseCases use_cases{fixture.projects, fixture.memberships, fixture.policy};
  transport::http::MembershipController controller{use_cases};
  const auto added_user = domain::Uuid::generate();

  const auto added = controller.add(
      principal(fixture.owner), fixture.projects.project.id.to_string(),
      nlohmann::json{{"user_id", added_user.to_string()}, {"role", "member"}}.dump());
  EXPECT_EQ(added.status, 201);
  EXPECT_EQ(nlohmann::json::parse(added.body)["role"], "member");

  const auto changed =
      controller.change_role(principal(fixture.owner), fixture.projects.project.id.to_string(),
                             added_user.to_string(), R"({"role":"manager"})");
  EXPECT_EQ(changed.status, 200);
  EXPECT_EQ(nlohmann::json::parse(changed.body)["role"], "manager");
  EXPECT_EQ(nlohmann::json::parse(
                controller.list(principal(fixture.member), fixture.projects.project.id.to_string())
                    .body)["items"]
                .size(),
            4U);
  EXPECT_EQ(controller
                .remove(principal(fixture.owner), fixture.projects.project.id.to_string(),
                        added_user.to_string())
                .status,
            204);
}

TEST(MembershipUseCasesTest, ManagerCannotGrantOrRevokeOwnerRole) {
  MembershipFixture fixture;
  application::MembershipUseCases use_cases{fixture.projects, fixture.memberships, fixture.policy};
  transport::http::MembershipController controller{use_cases};

  EXPECT_EQ(controller
                .change_role(principal(fixture.manager), fixture.projects.project.id.to_string(),
                             fixture.member.to_string(), R"({"role":"owner"})")
                .status,
            403);
  EXPECT_EQ(controller
                .remove(principal(fixture.manager), fixture.projects.project.id.to_string(),
                        fixture.owner.to_string())
                .status,
            403);
  EXPECT_EQ(controller
                .change_role(principal(fixture.manager), fixture.projects.project.id.to_string(),
                             fixture.member.to_string(), R"({"role":"manager"})")
                .status,
            200);
}

TEST(MembershipUseCasesTest, HidesMembershipListFromNonMembersAndRejectsDuplicates) {
  MembershipFixture fixture;
  application::MembershipUseCases use_cases{fixture.projects, fixture.memberships, fixture.policy};
  transport::http::MembershipController controller{use_cases};

  EXPECT_EQ(
      controller.list(principal(domain::Uuid::generate()), fixture.projects.project.id.to_string())
          .status,
      404);
  EXPECT_EQ(
      controller
          .add(principal(fixture.owner), fixture.projects.project.id.to_string(),
               nlohmann::json{{"user_id", fixture.member.to_string()}, {"role", "member"}}.dump())
          .status,
      409);
}

TEST(MembershipUseCasesTest, EnforcesMembershipContractForEveryRole) {
  MembershipFixture fixture;
  application::MembershipUseCases use_cases{fixture.projects, fixture.memberships, fixture.policy};
  transport::http::MembershipController controller{use_cases};
  const auto outsider = domain::Uuid::generate();
  const auto admin = domain::Uuid::generate();
  const auto candidate = domain::Uuid::generate();
  const auto body = nlohmann::json{{"user_id", candidate.to_string()}, {"role", "member"}}.dump();

  EXPECT_EQ(controller.add(principal(fixture.owner), fixture.projects.project.id.to_string(), body)
                .status,
            201);
  EXPECT_EQ(controller
                .change_role(principal(fixture.manager), fixture.projects.project.id.to_string(),
                             candidate.to_string(), R"({"role":"manager"})")
                .status,
            200);
  EXPECT_EQ(
      controller
          .add(principal(fixture.member), fixture.projects.project.id.to_string(),
               nlohmann::json{{"user_id", domain::Uuid::generate().to_string()}, {"role", "member"}}
                   .dump())
          .status,
      403);
  EXPECT_EQ(controller.list(principal(outsider), fixture.projects.project.id.to_string()).status,
            404);
  EXPECT_EQ(controller
                .list(principal(admin, domain::GlobalRole::admin),
                      fixture.projects.project.id.to_string())
                .status,
            200);
  EXPECT_EQ(controller
                .remove(principal(admin, domain::GlobalRole::admin),
                        fixture.projects.project.id.to_string(), candidate.to_string())
                .status,
            403);
}

TEST(MembershipUseCasesTest, RejectsMembershipMutationsAfterArchive) {
  MembershipFixture fixture;
  application::MembershipUseCases use_cases{fixture.projects, fixture.memberships, fixture.policy};
  transport::http::MembershipController controller{use_cases};
  fixture.projects.project.archived_at = now();
  fixture.projects.project.archived_by = fixture.owner;

  EXPECT_EQ(
      controller
          .add(principal(fixture.owner), fixture.projects.project.id.to_string(),
               nlohmann::json{{"user_id", domain::Uuid::generate().to_string()}, {"role", "member"}}
                   .dump())
          .status,
      409);
  EXPECT_EQ(
      controller.list(principal(fixture.member), fixture.projects.project.id.to_string()).status,
      200);
}

} // namespace
