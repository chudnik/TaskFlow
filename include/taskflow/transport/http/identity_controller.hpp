#pragma once

#include "taskflow/application/authentication_middleware.hpp"
#include "taskflow/application/authentication_sessions.hpp"
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
  IdentityController(const application::AuthenticationSessionUseCases &sessions,
                     const application::AuthenticationMiddleware &authentication);
  [[nodiscard]] ControllerResponse register_user(std::string_view json_body) const;
  [[nodiscard]] ControllerResponse login(std::string_view json_body) const;
  [[nodiscard]] ControllerResponse refresh(std::string_view json_body) const;
  [[nodiscard]] ControllerResponse logout(std::string_view authorization) const;

private:
  [[nodiscard]] ControllerResponse invoke(std::string_view json_body, bool registration) const;
  const application::IdentityUseCases *use_cases_;
  const application::AuthenticationSessionUseCases *sessions_{nullptr};
  const application::AuthenticationMiddleware *authentication_{nullptr};
};

} // namespace taskflow::transport::http
