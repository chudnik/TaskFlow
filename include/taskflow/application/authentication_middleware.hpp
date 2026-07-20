#pragma once

#include "taskflow/application/authentication.hpp"

#include <optional>
#include <string_view>

namespace taskflow::application {

class AuthenticationMiddleware {
public:
  explicit AuthenticationMiddleware(const AccessTokenService &tokens);
  [[nodiscard]] std::optional<AuthenticatedPrincipal>
  authenticate_bearer(std::string_view authorization) const noexcept;

private:
  const AccessTokenService *tokens_;
};

} // namespace taskflow::application
