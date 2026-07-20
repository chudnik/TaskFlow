#include "taskflow/infrastructure/identity_repositories.hpp"

#include <stdexcept>
#include <utility>

namespace taskflow::infrastructure {
namespace {

[[nodiscard]] const std::string &required(const QueryResult &result, const std::size_t row,
                                          const std::size_t column) {
  const auto &value = result.value(row, column);
  if (!value) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "identity row contains null"};
  }
  return *value;
}

[[nodiscard]] domain::Uuid uuid(const std::string &value) {
  auto parsed = domain::Uuid::parse(value);
  if (!parsed) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "identity row contains invalid UUID"};
  }
  return *parsed;
}

[[nodiscard]] domain::UtcInstant instant(const std::string &value) {
  auto parsed = domain::parse_utc(value);
  if (!parsed) {
    throw RepositoryError{RepositoryErrorCode::unexpected,
                          "identity row contains invalid timestamp"};
  }
  return *parsed;
}

[[nodiscard]] domain::User user_from(const QueryResult &result, const std::size_t row = 0) {
  return domain::User{
      uuid(required(result, row, 0)), required(result, row, 1),
      required(result, row, 2) == "admin" ? domain::GlobalRole::admin : domain::GlobalRole::user,
      required(result, row, 3) == "active" ? domain::AccountStatus::active
                                            : domain::AccountStatus::inactive,
      instant(required(result, row, 4)), instant(required(result, row, 5))};
}

[[nodiscard]] domain::Session session_from(const QueryResult &result,
                                            const std::size_t row = 0) {
  return domain::Session{uuid(required(result, row, 0)), uuid(required(result, row, 1)),
                         uuid(required(result, row, 2)), instant(required(result, row, 3)),
                         instant(required(result, row, 4)), instant(required(result, row, 5)),
                         result.value(row, 6).has_value()};
}

constexpr std::string_view user_columns =
    "id::text, email, global_role, account_status, "
    "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"')";
constexpr std::string_view session_columns =
    "id::text, user_id::text, token_family_id::text, "
    "to_char(expires_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "to_char(last_rotated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "revoked_at";

} // namespace

UserRepository::UserRepository(PostgresConnection &connection) : connection_{&connection} {}

domain::User UserRepository::create(std::string normalized_email, std::string password_hash) {
  const auto id = domain::Uuid::generate();
  const auto result = connection_->execute(
      "INSERT INTO users(id, email, password_hash) VALUES($1::uuid, $2, $3) RETURNING " +
          std::string{user_columns},
      {id.to_string(), std::move(normalized_email), std::move(password_hash)});
  return user_from(result);
}

std::optional<domain::User> UserRepository::find_by_id(const domain::Uuid &id) {
  const auto result = connection_->execute("SELECT " + std::string{user_columns} +
                                               " FROM users WHERE id = $1::uuid",
                                           {id.to_string()});
  return result.row_count() == 0 ? std::nullopt
                                 : std::optional<domain::User>{user_from(result)};
}

std::optional<UserCredentialRecord>
UserRepository::find_credentials_by_email(const std::string_view normalized_email) {
  const auto result = connection_->execute("SELECT " + std::string{user_columns} +
                                               ", password_hash FROM users WHERE email = $1",
                                           {std::string{normalized_email}});
  return result.row_count() == 0
             ? std::nullopt
             : std::optional<UserCredentialRecord>{
                   UserCredentialRecord{user_from(result), required(result, 0, 6)}};
}

SessionRepository::SessionRepository(PostgresConnection &connection) : connection_{&connection} {}

domain::Session SessionRepository::create(const domain::Uuid &user_id,
                                          const domain::Uuid &token_family_id,
                                          std::string refresh_token_hash,
                                          const domain::UtcInstant expires_at) {
  const auto id = domain::Uuid::generate();
  const auto result = connection_->execute(
      "INSERT INTO sessions(id, user_id, token_family_id, refresh_token_hash, expires_at) "
      "VALUES($1::uuid, $2::uuid, $3::uuid, $4, $5::timestamptz) RETURNING " +
          std::string{session_columns},
      {id.to_string(), user_id.to_string(), token_family_id.to_string(),
       std::move(refresh_token_hash), domain::format_utc(expires_at)});
  return session_from(result);
}

std::optional<domain::Session> SessionRepository::find_by_id(const domain::Uuid &id) {
  const auto result = connection_->execute("SELECT " + std::string{session_columns} +
                                               " FROM sessions WHERE id = $1::uuid",
                                           {id.to_string()});
  return result.row_count() == 0 ? std::nullopt
                                 : std::optional<domain::Session>{session_from(result)};
}

std::optional<SessionCredentialRecord>
SessionRepository::find_by_refresh_hash(const std::string_view refresh_token_hash) {
  const auto result = connection_->execute("SELECT " + std::string{session_columns} +
                                               ", refresh_token_hash FROM sessions "
                                               "WHERE refresh_token_hash = $1",
                                           {std::string{refresh_token_hash}});
  return result.row_count() == 0
             ? std::nullopt
             : std::optional<SessionCredentialRecord>{
                   SessionCredentialRecord{session_from(result), required(result, 0, 7)}};
}

} // namespace taskflow::infrastructure
