#pragma once

#include "taskflow/application/projects.hpp"
#include "taskflow/domain/project.hpp"
#include "taskflow/infrastructure/postgres.hpp"

#include <optional>
#include <string>
#include <vector>

namespace taskflow::infrastructure {

class ProjectRepository final : public application::ProjectStore {
public:
  explicit ProjectRepository(PostgresConnection &connection);

  [[nodiscard]] domain::Project create_with_owner(std::string name, std::string description,
                                                  const domain::Uuid &owner_id);
  [[nodiscard]] domain::Project create_project(std::string name, std::string description,
                                               const domain::Uuid &owner_id) override;
  [[nodiscard]] std::optional<domain::Project> find_by_id(const domain::Uuid &project_id);
  [[nodiscard]] std::optional<domain::Project>
  find_project(const domain::Uuid &project_id) override;
  [[nodiscard]] std::optional<domain::Project> find_visible_project(const domain::Uuid &project_id,
                                                                    const domain::Uuid &user_id,
                                                                    bool include_all) override;
  [[nodiscard]] std::optional<domain::ProjectRole> find_role(const domain::Uuid &project_id,
                                                             const domain::Uuid &user_id) override;
  [[nodiscard]] domain::Project update_project(const domain::Uuid &project_id, std::string name,
                                               std::string description) override;
  [[nodiscard]] domain::Project archive_project(const domain::Uuid &project_id,
                                                const domain::Uuid &actor_id) override;
  [[nodiscard]] std::vector<domain::Project> list_projects(const domain::Uuid &user_id,
                                                           bool include_all) override;

private:
  PostgresConnection *connection_;
};

class ProjectMembershipRepository final : public application::MembershipStore {
public:
  explicit ProjectMembershipRepository(PostgresConnection &connection);

  [[nodiscard]] domain::ProjectMembership
  add(const domain::Uuid &project_id, const domain::Uuid &user_id, domain::ProjectRole role);
  [[nodiscard]] domain::ProjectMembership add_membership(const domain::Uuid &project_id,
                                                         const domain::Uuid &user_id,
                                                         domain::ProjectRole role) override;
  [[nodiscard]] std::optional<domain::ProjectMembership> find(const domain::Uuid &project_id,
                                                              const domain::Uuid &user_id);
  [[nodiscard]] std::optional<domain::ProjectMembership>
  find_membership(const domain::Uuid &project_id, const domain::Uuid &user_id) override;
  [[nodiscard]] std::vector<domain::ProjectMembership> list(const domain::Uuid &project_id);
  [[nodiscard]] std::vector<domain::ProjectMembership>
  list_memberships(const domain::Uuid &project_id) override;
  [[nodiscard]] domain::ProjectMembership change_role(const domain::Uuid &project_id,
                                                      const domain::Uuid &user_id,
                                                      domain::ProjectRole role);
  [[nodiscard]] domain::ProjectMembership change_membership_role(const domain::Uuid &project_id,
                                                                 const domain::Uuid &user_id,
                                                                 domain::ProjectRole role) override;
  void remove(const domain::Uuid &project_id, const domain::Uuid &user_id);
  void remove_membership(const domain::Uuid &project_id, const domain::Uuid &user_id) override;

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
