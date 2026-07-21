#pragma once

#include "taskflow/domain/common.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace taskflow::domain {

enum class ProjectRole { owner, manager, member };

struct Project {
  Uuid id;
  std::string name;
  std::string description;
  Uuid created_by;
  UtcInstant created_at;
  UtcInstant updated_at;
  std::optional<UtcInstant> archived_at;
  std::optional<Uuid> archived_by;

  [[nodiscard]] bool archived() const noexcept { return archived_at.has_value(); }
};

struct ProjectMembership {
  Uuid project_id;
  Uuid user_id;
  ProjectRole role;
  UtcInstant joined_at;
  UtcInstant updated_at;
};

[[nodiscard]] ValidationErrors validate_project_fields(std::string_view name,
                                                       std::string_view description);
[[nodiscard]] std::string_view project_role_name(ProjectRole role) noexcept;
[[nodiscard]] std::optional<ProjectRole> parse_project_role(std::string_view value) noexcept;

} // namespace taskflow::domain
