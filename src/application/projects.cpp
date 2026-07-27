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
  auto project = store_->find_visible_project(project_id, actor.user_id,
                                              actor.global_role == domain::GlobalRole::admin);
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

MembershipUseCases::MembershipUseCases(ProjectStore &projects, MembershipStore &memberships,
                                       const PolicyService &policy)
    : projects_{&projects}, memberships_{&memberships}, policy_{&policy} {}

domain::Project MembershipUseCases::authorize_project(const AuthenticatedPrincipal &actor,
                                                      const domain::Uuid &project_id,
                                                      const ProjectAction action) const {
  const auto project = projects_->find_visible_project(
      project_id, actor.user_id, actor.global_role == domain::GlobalRole::admin);
  if (!project) {
    throw ProjectError{ProjectErrorCode::not_found, "project not found"};
  }
  const AuthorizationContext context{actor.global_role,
                                     projects_->find_role(project_id, actor.user_id)};
  if (!policy_->permits(context, action)) {
    throw ProjectError{action == ProjectAction::read_project ? ProjectErrorCode::not_found
                                                             : ProjectErrorCode::forbidden,
                       action == ProjectAction::read_project ? "project not found"
                                                             : "membership action is forbidden"};
  }
  if (project->archived() && action != ProjectAction::read_project) {
    throw ProjectError{ProjectErrorCode::archived, "archived project membership cannot be changed"};
  }
  return *project;
}

void MembershipUseCases::authorize_role_change(
    const AuthenticatedPrincipal &actor, const domain::Uuid &project_id,
    const std::optional<domain::ProjectRole> current_role,
    const domain::ProjectRole new_role) const {
  const auto actor_role = projects_->find_role(project_id, actor.user_id);
  const AuthorizationContext context{actor.global_role, actor_role};
  const bool touches_owner =
      new_role == domain::ProjectRole::owner || current_role == domain::ProjectRole::owner;
  const auto action =
      touches_owner ? ProjectAction::manage_owner_role : ProjectAction::manage_members;
  if (!policy_->permits(context, action)) {
    throw ProjectError{ProjectErrorCode::forbidden,
                       "owner membership can only be changed by an owner"};
  }
}

domain::ProjectMembership MembershipUseCases::add(const AuthenticatedPrincipal &actor,
                                                  const domain::Uuid &project_id,
                                                  const domain::Uuid &user_id,
                                                  const domain::ProjectRole role) const {
  static_cast<void>(authorize_project(actor, project_id, ProjectAction::manage_members));
  authorize_role_change(actor, project_id, std::nullopt, role);
  if (memberships_->find_membership(project_id, user_id)) {
    throw ProjectError{ProjectErrorCode::conflict, "user is already a project member"};
  }
  return memberships_->add_membership(project_id, user_id, role);
}

domain::ProjectMembership MembershipUseCases::change_role(const AuthenticatedPrincipal &actor,
                                                          const domain::Uuid &project_id,
                                                          const domain::Uuid &user_id,
                                                          const domain::ProjectRole role) const {
  static_cast<void>(authorize_project(actor, project_id, ProjectAction::manage_members));
  const auto current = memberships_->find_membership(project_id, user_id);
  if (!current) {
    throw ProjectError{ProjectErrorCode::not_found, "project membership not found"};
  }
  authorize_role_change(actor, project_id, current->role, role);
  return memberships_->change_membership_role(project_id, user_id, role);
}

std::vector<domain::ProjectMembership>
MembershipUseCases::list(const AuthenticatedPrincipal &actor,
                         const domain::Uuid &project_id) const {
  static_cast<void>(authorize_project(actor, project_id, ProjectAction::read_project));
  return memberships_->list_memberships(project_id);
}

void MembershipUseCases::remove(const AuthenticatedPrincipal &actor, const domain::Uuid &project_id,
                                const domain::Uuid &user_id) const {
  static_cast<void>(authorize_project(actor, project_id, ProjectAction::manage_members));
  const auto current = memberships_->find_membership(project_id, user_id);
  if (!current) {
    throw ProjectError{ProjectErrorCode::not_found, "project membership not found"};
  }
  authorize_role_change(actor, project_id, current->role, domain::ProjectRole::member);
  memberships_->remove_membership(project_id, user_id);
}

} // namespace taskflow::application
