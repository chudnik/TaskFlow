#pragma once

#include "taskflow/application/audit.hpp"
#include "taskflow/infrastructure/postgres.hpp"

namespace taskflow::infrastructure {

class AuditRepository final : public application::AuditStore {
public:
  explicit AuditRepository(PostgresConnection &connection);
  [[nodiscard]] application::AuditEvent append_audit(application::AuditEvent event) override;
  [[nodiscard]] application::AuditPage list_audit(const domain::Uuid &project_id,
                                                  std::optional<domain::Uuid> task_id,
                                                  std::optional<std::uint64_t> before_id,
                                                  std::size_t limit) override;

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
