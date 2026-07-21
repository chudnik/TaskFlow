#include "taskflow/domain/project.hpp"

namespace taskflow::domain {

ValidationErrors validate_project_fields(const std::string_view name,
                                         const std::string_view description) {
  ValidationErrors errors;
  errors.require_text("name", name, 1, 200);
  if (description.size() > 10'000) {
    errors.add("description", "too_long", "description must contain at most 10000 bytes");
  }
  return errors;
}

std::string_view project_role_name(const ProjectRole role) noexcept {
  switch (role) {
  case ProjectRole::owner:
    return "owner";
  case ProjectRole::manager:
    return "manager";
  case ProjectRole::member:
    return "member";
  }
  return "member";
}

std::optional<ProjectRole> parse_project_role(const std::string_view value) noexcept {
  if (value == "owner") {
    return ProjectRole::owner;
  }
  if (value == "manager") {
    return ProjectRole::manager;
  }
  if (value == "member") {
    return ProjectRole::member;
  }
  return std::nullopt;
}

} // namespace taskflow::domain
