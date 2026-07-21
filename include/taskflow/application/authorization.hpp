#pragma once

#include "taskflow/domain/identity_models.hpp"
#include "taskflow/domain/project.hpp"

#include <optional>

namespace taskflow::application {

using ProjectRole = domain::ProjectRole;

enum class ProjectAction {
  read_project,
  update_project,
  archive_project,
  create_task,
  update_task,
  delete_task,
  manage_members,
  manage_owner_role,
  moderate_comment,
  admin_moderation,
};

struct AuthorizationContext {
  domain::GlobalRole global_role;
  std::optional<ProjectRole> project_role;
};

class PolicyService {
public:
  [[nodiscard]] bool permits(const AuthorizationContext &context,
                             ProjectAction action) const noexcept;
};

} // namespace taskflow::application
