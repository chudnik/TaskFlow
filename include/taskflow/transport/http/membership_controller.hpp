#pragma once

#include "taskflow/application/projects.hpp"
#include "taskflow/transport/http/identity_controller.hpp"

#include <string_view>

namespace taskflow::transport::http {

class MembershipController {
public:
  explicit MembershipController(const application::MembershipUseCases &use_cases);

  [[nodiscard]] ControllerResponse add(const application::AuthenticatedPrincipal &actor,
                                       std::string_view project_id,
                                       std::string_view json_body) const;
  [[nodiscard]] ControllerResponse change_role(const application::AuthenticatedPrincipal &actor,
                                               std::string_view project_id,
                                               std::string_view user_id,
                                               std::string_view json_body) const;
  [[nodiscard]] ControllerResponse list(const application::AuthenticatedPrincipal &actor,
                                        std::string_view project_id) const;
  [[nodiscard]] ControllerResponse remove(const application::AuthenticatedPrincipal &actor,
                                          std::string_view project_id,
                                          std::string_view user_id) const;

private:
  const application::MembershipUseCases *use_cases_;
};

} // namespace taskflow::transport::http
