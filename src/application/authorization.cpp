#include "taskflow/application/authorization.hpp"

namespace taskflow::application {

bool PolicyService::permits(const AuthorizationContext &context,
                            const ProjectAction action) const noexcept {
  if (context.global_role == domain::GlobalRole::admin) {
    return action == ProjectAction::read_project || action == ProjectAction::admin_moderation;
  }
  if (!context.project_role) {
    return false;
  }
  switch (action) {
  case ProjectAction::read_project:
  case ProjectAction::create_task:
    return true;
  case ProjectAction::update_project:
  case ProjectAction::update_task:
  case ProjectAction::manage_members:
  case ProjectAction::moderate_comment:
    return *context.project_role == ProjectRole::owner ||
           *context.project_role == ProjectRole::manager;
  case ProjectAction::archive_project:
  case ProjectAction::manage_owner_role:
    return *context.project_role == ProjectRole::owner;
  case ProjectAction::delete_task:
    return *context.project_role == ProjectRole::owner ||
           *context.project_role == ProjectRole::manager;
  case ProjectAction::admin_moderation:
    return false;
  }
  return false;
}

} // namespace taskflow::application
