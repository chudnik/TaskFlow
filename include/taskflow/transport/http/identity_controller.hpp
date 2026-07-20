#pragma once

#include "taskflow/application/identity.hpp"

#include <string>
#include <string_view>

namespace taskflow::transport::http {

struct ControllerResponse {
  int status;
  std::string body;
};

class IdentityController {
public:
  explicit IdentityController(const application::IdentityUseCases &use_cases);
  [[nodiscard]] ControllerResponse register_user(std::string_view json_body) const;
  [[nodiscard]] ControllerResponse login(std::string_view json_body) const;

private:
  [[nodiscard]] ControllerResponse invoke(std::string_view json_body, bool registration) const;
  const application::IdentityUseCases *use_cases_;
};

} // namespace taskflow::transport::http
