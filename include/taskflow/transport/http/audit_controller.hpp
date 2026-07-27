#pragma once

#include "taskflow/application/audit.hpp"
#include "taskflow/application/tasks.hpp"
#include "taskflow/transport/http/identity_controller.hpp"

namespace taskflow::transport::http {

class AuditController {
public:
  explicit AuditController(const application::AuditUseCases &use_cases);
  AuditController(const application::AuditUseCases &use_cases, application::TaskStore &tasks);
  [[nodiscard]] ControllerResponse history(const application::AuthenticatedPrincipal &actor,
                                           std::string_view project_id,
                                           std::optional<std::string_view> task_id,
                                           domain::PageRequest page) const;
  [[nodiscard]] ControllerResponse task_history(const application::AuthenticatedPrincipal &actor,
                                                std::string_view task_id,
                                                domain::PageRequest page) const;

private:
  const application::AuditUseCases *use_cases_;
  application::TaskStore *tasks_{nullptr};
};

} // namespace taskflow::transport::http
