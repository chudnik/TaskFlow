#include "taskflow/application/audit.hpp"
#include "taskflow/application/task_cursor.hpp"
#include "taskflow/application/tasks.hpp"

#include <algorithm>
#include <cctype>

namespace taskflow::application {

std::map<std::string, std::string>
sanitize_audit_fields(std::map<std::string, std::string> fields) {
  for (auto &[key, value] : fields) {
    std::string normalized = key;
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(),
        [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
    if (normalized.find("password") != std::string::npos ||
        normalized.find("token") != std::string::npos ||
        normalized.find("secret") != std::string::npos)
      value = "[REDACTED]";
  }
  return fields;
}

AuditUseCases::AuditUseCases(ProjectStore &projects, AuditStore &audit)
    : projects_{&projects}, audit_{&audit} {}

AuditPage AuditUseCases::history(const AuthenticatedPrincipal &actor,
                                 const domain::Uuid &project_id,
                                 const std::optional<domain::Uuid> task_id,
                                 const domain::PageRequest page) const {
  if (!projects_->find_visible_project(project_id, actor.user_id, false))
    throw TaskError{TaskErrorCode::not_found, "history not found"};
  std::optional<std::uint64_t> before;
  if (page.cursor) {
    try {
      before = std::stoull(*page.cursor);
    } catch (const std::exception &) {
      throw CursorError{CursorErrorCode::malformed, "history cursor is malformed"};
    }
  }
  return audit_->list_audit(project_id, task_id, before, page.size);
}

} // namespace taskflow::application
