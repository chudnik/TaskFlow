#pragma once

#include "taskflow/application/authentication.hpp"
#include "taskflow/application/authentication_sessions.hpp"
#include "taskflow/infrastructure/identity_repositories.hpp"
#include "taskflow/infrastructure/refresh_tokens.hpp"

namespace taskflow::infrastructure {

class PostgresAccountSessionValidator final : public application::AccountSessionValidator {
public:
  explicit PostgresAccountSessionValidator(PostgresConnection &connection);

  [[nodiscard]] bool account_and_session_active(const domain::Uuid &user_id,
                                                const domain::Uuid &session_id) const override;

private:
  PostgresConnection *connection_;
};

class PostgresAuthenticationSessionStore final : public application::AuthenticationSessionStore {
public:
  PostgresAuthenticationSessionStore(RefreshTokenService &refresh_tokens,
                                     SessionRepository &sessions, UserRepository &users);

  [[nodiscard]] application::IssuedSession create(const domain::User &user,
                                                  domain::UtcInstant expires_at) override;
  [[nodiscard]] application::SessionRotation rotate(std::string_view refresh_token,
                                                    domain::UtcInstant expires_at) override;
  void logout(const domain::Uuid &session_id) override;

private:
  RefreshTokenService *refresh_tokens_;
  SessionRepository *sessions_;
  UserRepository *users_;
};

} // namespace taskflow::infrastructure
