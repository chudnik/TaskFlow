#pragma once

#include "taskflow/domain/identity_models.hpp"
#include "taskflow/infrastructure/postgres.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace taskflow::infrastructure {

struct UserCredentialRecord {
  domain::User user;
  std::string password_hash;
};

struct SessionCredentialRecord {
  domain::Session session;
  std::string refresh_token_hash;
};

class UserRepository {
public:
  explicit UserRepository(PostgresConnection &connection);
  [[nodiscard]] domain::User create(std::string normalized_email, std::string password_hash);
  [[nodiscard]] std::optional<domain::User> find_by_id(const domain::Uuid &id);
  [[nodiscard]] std::optional<UserCredentialRecord>
  find_credentials_by_email(std::string_view normalized_email);

private:
  PostgresConnection *connection_;
};

class SessionRepository {
public:
  explicit SessionRepository(PostgresConnection &connection);
  [[nodiscard]] domain::Session create(const domain::Uuid &user_id,
                                       const domain::Uuid &token_family_id,
                                       std::string refresh_token_hash,
                                       domain::UtcInstant expires_at);
  [[nodiscard]] std::optional<domain::Session> find_by_id(const domain::Uuid &id);
  [[nodiscard]] std::optional<SessionCredentialRecord>
  find_by_refresh_hash(std::string_view refresh_token_hash);

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
