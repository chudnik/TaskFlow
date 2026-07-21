#include "taskflow/application/projects.hpp"

#include <utility>

namespace taskflow::application {

ProjectError::ProjectError(const ProjectErrorCode code, std::string message)
    : std::runtime_error{std::move(message)}, code_{code} {}

ProjectErrorCode ProjectError::code() const noexcept { return code_; }

ProjectUseCases::ProjectUseCases(ProjectStore &store, const PolicyService &policy)
    : store_{&store}, policy_{&policy} {}

domain::Project ProjectUseCases::create(const AuthenticatedPrincipal &actor, std::string name,
                                        std::string description) const {
  const auto errors = domain::validate_project_fields(name, description);
  if (!errors.empty()) {
    throw ProjectError{ProjectErrorCode::invalid_input, errors.items().front().message};
  }
  return store_->create_project(std::move(name), std::move(description), actor.user_id);
}

domain::Project ProjectUseCases::authorize(const AuthenticatedPrincipal &actor,
                                           const domain::Uuid &project_id,
                                           const ProjectAction action) const {
  auto project = store_->find_project(project_id);
  if (!project) {
    throw ProjectError{ProjectErrorCode::not_found, "project not found"};
  }
  const AuthorizationContext context{actor.global_role,
                                     store_->find_role(project_id, actor.user_id)};
  if (!policy_->permits(context, action)) {
    throw ProjectError{action == ProjectAction::read_project ? ProjectErrorCode::not_found
                                                             : ProjectErrorCode::forbidden,
                       action == ProjectAction::read_project ? "project not found"
                                                             : "project action is forbidden"};
  }
  return *project;
}

domain::Project ProjectUseCases::read(const AuthenticatedPrincipal &actor,
                                      const domain::Uuid &project_id) const {
  return authorize(actor, project_id, ProjectAction::read_project);
}

domain::Project ProjectUseCases::update(const AuthenticatedPrincipal &actor,
                                        const domain::Uuid &project_id, std::string name,
                                        std::string description) const {
  const auto project = authorize(actor, project_id, ProjectAction::update_project);
  if (project.archived()) {
    throw ProjectError{ProjectErrorCode::archived, "archived project cannot be changed"};
  }
  const auto errors = domain::validate_project_fields(name, description);
  if (!errors.empty()) {
    throw ProjectError{ProjectErrorCode::invalid_input, errors.items().front().message};
  }
  return store_->update_project(project_id, std::move(name), std::move(description));
}

domain::Project ProjectUseCases::archive(const AuthenticatedPrincipal &actor,
                                         const domain::Uuid &project_id) const {
  const auto project = authorize(actor, project_id, ProjectAction::archive_project);
  if (project.archived()) {
    throw ProjectError{ProjectErrorCode::archived, "project is already archived"};
  }
  return store_->archive_project(project_id, actor.user_id);
}

std::vector<domain::Project> ProjectUseCases::list(const AuthenticatedPrincipal &actor) const {
  return store_->list_projects(actor.user_id, actor.global_role == domain::GlobalRole::admin);
}

} // namespace taskflow::application
