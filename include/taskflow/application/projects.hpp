#pragma once

#include "taskflow/application/authentication.hpp"
#include "taskflow/application/authorization.hpp"
#include "taskflow/domain/project.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace taskflow::application {

enum class ProjectErrorCode { invalid_input, not_found, forbidden, archived };

class ProjectError : public std::runtime_error {
public:
  ProjectError(ProjectErrorCode code, std::string message);
  [[nodiscard]] ProjectErrorCode code() const noexcept;

private:
  ProjectErrorCode code_;
};

class ProjectStore {
public:
  virtual ~ProjectStore() = default;
  [[nodiscard]] virtual domain::Project create_project(std::string name, std::string description,
                                                       const domain::Uuid &owner_id) = 0;
  [[nodiscard]] virtual std::optional<domain::Project>
  find_project(const domain::Uuid &project_id) = 0;
  [[nodiscard]] virtual std::optional<domain::ProjectRole>
  find_role(const domain::Uuid &project_id, const domain::Uuid &user_id) = 0;
  [[nodiscard]] virtual domain::Project
  update_project(const domain::Uuid &project_id, std::string name, std::string description) = 0;
  [[nodiscard]] virtual domain::Project archive_project(const domain::Uuid &project_id,
                                                        const domain::Uuid &actor_id) = 0;
  [[nodiscard]] virtual std::vector<domain::Project> list_projects(const domain::Uuid &user_id,
                                                                   bool include_all) = 0;
};

class ProjectUseCases {
public:
  ProjectUseCases(ProjectStore &store, const PolicyService &policy);

  [[nodiscard]] domain::Project create(const AuthenticatedPrincipal &actor, std::string name,
                                       std::string description) const;
  [[nodiscard]] domain::Project read(const AuthenticatedPrincipal &actor,
                                     const domain::Uuid &project_id) const;
  [[nodiscard]] domain::Project update(const AuthenticatedPrincipal &actor,
                                       const domain::Uuid &project_id, std::string name,
                                       std::string description) const;
  [[nodiscard]] domain::Project archive(const AuthenticatedPrincipal &actor,
                                        const domain::Uuid &project_id) const;
  [[nodiscard]] std::vector<domain::Project> list(const AuthenticatedPrincipal &actor) const;

private:
  [[nodiscard]] domain::Project authorize(const AuthenticatedPrincipal &actor,
                                          const domain::Uuid &project_id,
                                          ProjectAction action) const;
  ProjectStore *store_;
  const PolicyService *policy_;
};

} // namespace taskflow::application
