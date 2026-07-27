#include "taskflow/infrastructure/refresh_tokens.hpp"

#include <openssl/sha.h>

#include <array>
#include <iomanip>
#include <random>
#include <sstream>

namespace taskflow::infrastructure {
namespace {
[[nodiscard]] const std::string &required(const QueryResult &result, const std::size_t column) {
  const auto &value = result.value(0, column);
  if (!value) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "refresh token row contains null"};
  }
  return *value;
}

[[nodiscard]] std::string base64url(const std::array<unsigned char, 32> &bytes) {
  static constexpr char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string output;
  output.reserve(43);
  std::uint32_t accumulator = 0;
  int bits = 0;
  for (const auto byte : bytes) {
    accumulator = (accumulator << 8U) | byte;
    bits += 8;
    while (bits >= 6) {
      bits -= 6;
      output.push_back(alphabet[(accumulator >> bits) & 0x3FU]);
    }
  }
  if (bits > 0) {
    output.push_back(alphabet[(accumulator << (6 - bits)) & 0x3FU]);
  }
  return output;
}
} // namespace

RefreshTokenService::RefreshTokenService(PostgresConnection &connection)
    : connection_{&connection} {}

std::string RefreshTokenService::generate_token() {
  std::random_device random;
  std::array<unsigned char, 32> bytes{};
  for (auto &byte : bytes) {
    byte = static_cast<unsigned char>(random());
  }
  return base64url(bytes);
}

std::string RefreshTokenService::hash_token(const std::string_view token) {
  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char *>(token.data()), token.size(), digest.data());
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const auto byte : digest) {
    output << std::setw(2) << static_cast<unsigned int>(byte);
  }
  return output.str();
}

IssuedRefreshToken RefreshTokenService::create_session(const domain::Uuid &user_id,
                                                       const domain::UtcInstant expires_at) {
  const auto session_id = domain::Uuid::generate();
  const auto family_id = domain::Uuid::generate();
  auto token = generate_token();
  const auto token_hash = hash_token(token);
  auto transaction = connection_->transaction();
  static_cast<void>(transaction.execute(
      "INSERT INTO sessions(id, user_id, token_family_id, refresh_token_hash, expires_at) "
      "VALUES($1::uuid, $2::uuid, $3::uuid, $4, $5::timestamptz)",
      {session_id.to_string(), user_id.to_string(), family_id.to_string(), token_hash,
       domain::format_utc(expires_at)}));
  static_cast<void>(transaction.execute(
      "INSERT INTO session_refresh_tokens(token_hash, session_id, token_family_id) "
      "VALUES($1, $2::uuid, $3::uuid)",
      {token_hash, session_id.to_string(), family_id.to_string()}));
  transaction.commit();
  return {session_id, std::move(token)};
}

RefreshRotationResult RefreshTokenService::rotate(const std::string_view presented_token) {
  const auto presented_hash = hash_token(presented_token);
  auto transaction = connection_->transaction();
  const auto current = transaction.execute(
      "SELECT t.session_id::text, t.token_family_id::text, t.used_at, t.revoked_at, "
      "s.expires_at <= clock_timestamp(), s.revoked_at "
      "FROM session_refresh_tokens t JOIN sessions s ON s.id = t.session_id "
      "WHERE t.token_hash = $1 FOR UPDATE OF t, s",
      {presented_hash});
  if (current.row_count() == 0) {
    transaction.rollback();
    return {RefreshRotationStatus::invalid, std::nullopt};
  }
  const auto session_id = required(current, 0);
  const auto family_id = required(current, 1);
  const bool replay = current.value(0, 2).has_value() || current.value(0, 3).has_value();
  const bool unavailable = required(current, 4) == "t" || current.value(0, 5).has_value();
  if (replay) {
    static_cast<void>(transaction.execute(
        "UPDATE sessions SET revoked_at = COALESCE(revoked_at, clock_timestamp()), "
        "revoke_reason = COALESCE(revoke_reason, 'refresh_token_replay') "
        "WHERE token_family_id = $1::uuid",
        {family_id}));
    static_cast<void>(transaction.execute(
        "UPDATE session_refresh_tokens SET revoked_at = COALESCE(revoked_at, clock_timestamp()) "
        "WHERE token_family_id = $1::uuid",
        {family_id}));
    transaction.commit();
    return {RefreshRotationStatus::replay_detected, std::nullopt};
  }
  if (unavailable) {
    transaction.rollback();
    return {RefreshRotationStatus::invalid, std::nullopt};
  }
  auto next_token = generate_token();
  const auto next_hash = hash_token(next_token);
  static_cast<void>(transaction.execute(
      "UPDATE session_refresh_tokens SET used_at = clock_timestamp() WHERE token_hash = $1",
      {presented_hash}));
  static_cast<void>(transaction.execute(
      "UPDATE sessions SET refresh_token_hash = $1, last_rotated_at = clock_timestamp() "
      "WHERE id = $2::uuid",
      {next_hash, session_id}));
  static_cast<void>(transaction.execute(
      "INSERT INTO session_refresh_tokens(token_hash, session_id, token_family_id) "
      "VALUES($1, $2::uuid, $3::uuid)",
      {next_hash, session_id, family_id}));
  transaction.commit();
  return {RefreshRotationStatus::rotated,
          IssuedRefreshToken{*domain::Uuid::parse(session_id), std::move(next_token)}};
}

void RefreshTokenService::logout(const domain::Uuid &session_id) {
  auto transaction = connection_->transaction();
  static_cast<void>(transaction.execute(
      "UPDATE sessions SET revoked_at = COALESCE(revoked_at, clock_timestamp()), "
      "revoke_reason = COALESCE(revoke_reason, 'logout') WHERE id = $1::uuid",
      {session_id.to_string()}));
  static_cast<void>(transaction.execute(
      "UPDATE session_refresh_tokens SET revoked_at = COALESCE(revoked_at, clock_timestamp()) "
      "WHERE session_id = $1::uuid",
      {session_id.to_string()}));
  transaction.commit();
}

void RefreshTokenService::revoke_user_sessions(const domain::Uuid &user_id,
                                               const std::string_view reason) {
  auto transaction = connection_->transaction();
  static_cast<void>(transaction.execute(
      "UPDATE sessions SET revoked_at = COALESCE(revoked_at, clock_timestamp()), "
      "revoke_reason = COALESCE(revoke_reason, $2) WHERE user_id = $1::uuid",
      {user_id.to_string(), std::string{reason}}));
  static_cast<void>(transaction.execute(
      "UPDATE session_refresh_tokens t SET revoked_at = COALESCE(t.revoked_at, clock_timestamp()) "
      "FROM sessions s WHERE s.id = t.session_id AND s.user_id = $1::uuid",
      {user_id.to_string()}));
  transaction.commit();
}

} // namespace taskflow::infrastructure
