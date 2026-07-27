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

enum class TaskAction { read, update, change_status, assign, remove };

struct AuthorizationContext {
  domain::GlobalRole global_role;
  std::optional<ProjectRole> project_role;
};

struct TaskAuthorizationContext {
  domain::GlobalRole global_role;
  std::optional<ProjectRole> project_role;
  bool is_creator;
  bool is_assignee;
};

class PolicyService {
public:
  [[nodiscard]] bool permits(const AuthorizationContext &context,
                             ProjectAction action) const noexcept;
  [[nodiscard]] bool permits(const TaskAuthorizationContext &context,
                             TaskAction action) const noexcept;
};

} // namespace taskflow::application
