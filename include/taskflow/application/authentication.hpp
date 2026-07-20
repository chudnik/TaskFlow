#pragma once

#include "taskflow/domain/identity_models.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace taskflow::application {

struct AuthenticatedPrincipal {
  domain::Uuid user_id;
  domain::Uuid session_id;
  domain::GlobalRole global_role;
  domain::UtcInstant expires_at;
};

class AccountSessionValidator {
public:
  virtual ~AccountSessionValidator() = default;
  [[nodiscard]] virtual bool account_and_session_active(const domain::Uuid &user_id,
                                                        const domain::Uuid &session_id) const = 0;
};

class AccessTokenService {
public:
  virtual ~AccessTokenService() = default;
  [[nodiscard]] virtual std::string create(const domain::User &user,
                                           const domain::Uuid &session_id) const = 0;
  [[nodiscard]] virtual std::optional<AuthenticatedPrincipal>
  validate(std::string_view token) const noexcept = 0;
};

} // namespace taskflow::application
