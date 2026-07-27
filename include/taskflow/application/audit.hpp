#pragma once

#include "taskflow/application/projects.hpp"

#include <map>

namespace taskflow::application {

struct AuditEvent {
  std::uint64_t id;
  domain::Uuid event_id;
  domain::Uuid project_id;
  std::optional<domain::Uuid> task_id;
  std::optional<domain::Uuid> actor_user_id;
  std::string event_type;
  std::string entity_type;
  domain::Uuid entity_id;
  std::map<std::string, std::string> before;
  std::map<std::string, std::string> after;
  std::string correlation_id;
  domain::UtcInstant occurred_at;
};

struct AuditPage {
  std::vector<AuditEvent> items;
  std::optional<std::uint64_t> next_before_id;
};

class AuditStore {
public:
  virtual ~AuditStore() = default;
  [[nodiscard]] virtual AuditEvent append_audit(AuditEvent event) = 0;
  [[nodiscard]] virtual AuditPage list_audit(const domain::Uuid &project_id,
                                             std::optional<domain::Uuid> task_id,
                                             std::optional<std::uint64_t> before_id,
                                             std::size_t limit) = 0;
};

[[nodiscard]] std::map<std::string, std::string>
sanitize_audit_fields(std::map<std::string, std::string> fields);

class AuditUseCases {
public:
  AuditUseCases(ProjectStore &projects, AuditStore &audit);
  [[nodiscard]] AuditPage history(const AuthenticatedPrincipal &actor,
                                  const domain::Uuid &project_id,
                                  std::optional<domain::Uuid> task_id,
                                  domain::PageRequest page) const;

private:
  ProjectStore *projects_;
  AuditStore *audit_;
};

} // namespace taskflow::application
