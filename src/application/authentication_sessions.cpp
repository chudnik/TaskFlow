#include "taskflow/application/authentication_sessions.hpp"

#include <utility>

namespace taskflow::application {

AuthenticationSessionError::AuthenticationSessionError(const AuthenticationSessionErrorCode code,
                                                       std::string message)
    : std::runtime_error{std::move(message)}, code_{code} {}

AuthenticationSessionErrorCode AuthenticationSessionError::code() const noexcept { return code_; }

AuthenticationSessionUseCases::AuthenticationSessionUseCases(
    const IdentityUseCases &identity, AuthenticationSessionStore &store,
    const AccessTokenService &access_tokens, const domain::Clock &clock)
    : identity_{&identity}, store_{&store}, access_tokens_{&access_tokens}, clock_{&clock} {}

AuthenticationTokens
AuthenticationSessionUseCases::register_user(const std::string_view email,
                                             const std::string_view password) const {
  return issue(identity_->register_user(email, password));
}

AuthenticationTokens AuthenticationSessionUseCases::login(const std::string_view email,
                                                          const std::string_view password) const {
  return issue(identity_->login(email, password));
}

AuthenticationTokens
AuthenticationSessionUseCases::refresh(const std::string_view refresh_token) const {
  const auto expires_at = clock_->now() + refresh_session_lifetime;
  auto rotation = store_->rotate(refresh_token, expires_at);
  if (rotation.status == SessionRotationStatus::replay_detected) {
    throw AuthenticationSessionError{AuthenticationSessionErrorCode::refresh_token_replay,
                                     "refresh token is invalid"};
  }
  if (rotation.status != SessionRotationStatus::rotated || !rotation.session) {
    throw AuthenticationSessionError{AuthenticationSessionErrorCode::invalid_refresh_token,
                                     "refresh token is invalid"};
  }
  return tokens_from(std::move(*rotation.session));
}

void AuthenticationSessionUseCases::logout(const domain::Uuid &session_id) const {
  store_->logout(session_id);
}

AuthenticationTokens AuthenticationSessionUseCases::issue(const domain::User &user) const {
  return tokens_from(store_->create(user, clock_->now() + refresh_session_lifetime));
}

AuthenticationTokens AuthenticationSessionUseCases::tokens_from(IssuedSession session) const {
  const auto access_expires_at = clock_->now() + access_token_lifetime;
  return AuthenticationTokens{.user = session.user,
                              .access_token =
                                  access_tokens_->create(session.user, session.session_id),
                              .refresh_token = std::move(session.refresh_token),
                              .access_expires_at = access_expires_at,
                              .refresh_expires_at = session.refresh_expires_at};
}

} // namespace taskflow::application
