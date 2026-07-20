#include "taskflow/application/authentication_middleware.hpp"

namespace taskflow::application {

AuthenticationMiddleware::AuthenticationMiddleware(const AccessTokenService &tokens)
    : tokens_{&tokens} {}

std::optional<AuthenticatedPrincipal>
AuthenticationMiddleware::authenticate_bearer(const std::string_view authorization) const noexcept {
  constexpr std::string_view prefix = "Bearer ";
  if (!authorization.starts_with(prefix) || authorization.size() == prefix.size() ||
      authorization.find_first_of(" \t\r\n", prefix.size()) != std::string_view::npos) {
    return std::nullopt;
  }
  return tokens_->validate(authorization.substr(prefix.size()));
}

} // namespace taskflow::application
