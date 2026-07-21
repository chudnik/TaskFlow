#pragma once

#include "taskflow/application/projects.hpp"
#include "taskflow/transport/http/identity_controller.hpp"

#include <string_view>

namespace taskflow::transport::http {

class ProjectController {
public:
  explicit ProjectController(const application::ProjectUseCases &use_cases);

  [[nodiscard]] ControllerResponse create(const application::AuthenticatedPrincipal &actor,
                                          std::string_view json_body) const;
  [[nodiscard]] ControllerResponse read(const application::AuthenticatedPrincipal &actor,
                                        std::string_view project_id) const;
  [[nodiscard]] ControllerResponse update(const application::AuthenticatedPrincipal &actor,
                                          std::string_view project_id,
                                          std::string_view json_body) const;
  [[nodiscard]] ControllerResponse archive(const application::AuthenticatedPrincipal &actor,
                                           std::string_view project_id) const;
  [[nodiscard]] ControllerResponse list(const application::AuthenticatedPrincipal &actor) const;

private:
  const application::ProjectUseCases *use_cases_;
};

} // namespace taskflow::transport::http
