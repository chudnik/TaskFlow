#pragma once

#include "taskflow/domain/project.hpp"
#include "taskflow/infrastructure/postgres.hpp"

#include <optional>
#include <string>
#include <vector>

namespace taskflow::infrastructure {

class ProjectRepository {
public:
  explicit ProjectRepository(PostgresConnection &connection);

  [[nodiscard]] domain::Project create_with_owner(std::string name, std::string description,
                                                  const domain::Uuid &owner_id);
  [[nodiscard]] std::optional<domain::Project> find_by_id(const domain::Uuid &project_id);

private:
  PostgresConnection *connection_;
};

class ProjectMembershipRepository {
public:
  explicit ProjectMembershipRepository(PostgresConnection &connection);

  [[nodiscard]] domain::ProjectMembership
  add(const domain::Uuid &project_id, const domain::Uuid &user_id, domain::ProjectRole role);
  [[nodiscard]] std::optional<domain::ProjectMembership> find(const domain::Uuid &project_id,
                                                              const domain::Uuid &user_id);
  [[nodiscard]] std::vector<domain::ProjectMembership> list(const domain::Uuid &project_id);
  [[nodiscard]] domain::ProjectMembership change_role(const domain::Uuid &project_id,
                                                      const domain::Uuid &user_id,
                                                      domain::ProjectRole role);
  void remove(const domain::Uuid &project_id, const domain::Uuid &user_id);

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
