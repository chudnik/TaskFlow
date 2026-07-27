#pragma once

#include "taskflow/application/authentication.hpp"
#include "taskflow/application/identity.hpp"
#include "taskflow/domain/common.hpp"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace taskflow::application {

inline constexpr auto access_token_lifetime = std::chrono::minutes{15};
inline constexpr auto refresh_session_lifetime = std::chrono::hours{24 * 30};

struct IssuedSession {
  domain::User user;
  domain::Uuid session_id;
  std::string refresh_token;
  domain::UtcInstant refresh_expires_at;
};

enum class SessionRotationStatus { rotated, invalid, replay_detected };

struct SessionRotation {
  SessionRotationStatus status;
  std::optional<IssuedSession> session;
};

class AuthenticationSessionStore {
public:
  virtual ~AuthenticationSessionStore() = default;
  [[nodiscard]] virtual IssuedSession create(const domain::User &user,
                                             domain::UtcInstant expires_at) = 0;
  [[nodiscard]] virtual SessionRotation rotate(std::string_view refresh_token,
                                               domain::UtcInstant expires_at) = 0;
  virtual void logout(const domain::Uuid &session_id) = 0;
};

enum class AuthenticationSessionErrorCode { invalid_refresh_token, refresh_token_replay };

class AuthenticationSessionError : public std::runtime_error {
public:
  AuthenticationSessionError(AuthenticationSessionErrorCode code, std::string message);
  [[nodiscard]] AuthenticationSessionErrorCode code() const noexcept;

private:
  AuthenticationSessionErrorCode code_;
};

struct AuthenticationTokens {
  domain::User user;
  std::string access_token;
  std::string refresh_token;
  domain::UtcInstant access_expires_at;
  domain::UtcInstant refresh_expires_at;
};

class AuthenticationSessionUseCases {
public:
  AuthenticationSessionUseCases(const IdentityUseCases &identity, AuthenticationSessionStore &store,
                                const AccessTokenService &access_tokens,
                                const domain::Clock &clock);

  [[nodiscard]] AuthenticationTokens register_user(std::string_view email,
                                                   std::string_view password) const;
  [[nodiscard]] AuthenticationTokens login(std::string_view email, std::string_view password) const;
  [[nodiscard]] AuthenticationTokens refresh(std::string_view refresh_token) const;
  void logout(const domain::Uuid &session_id) const;

private:
  [[nodiscard]] AuthenticationTokens issue(const domain::User &user) const;
  [[nodiscard]] AuthenticationTokens tokens_from(IssuedSession session) const;

  const IdentityUseCases *identity_;
  AuthenticationSessionStore *store_;
  const AccessTokenService *access_tokens_;
  const domain::Clock *clock_;
};

} // namespace taskflow::application
