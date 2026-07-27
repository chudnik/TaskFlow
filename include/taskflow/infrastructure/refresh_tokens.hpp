#pragma once

#include "taskflow/domain/common.hpp"
#include "taskflow/infrastructure/postgres.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace taskflow::infrastructure {

struct IssuedRefreshToken {
  domain::Uuid session_id;
  std::string token;
};

enum class RefreshRotationStatus { rotated, invalid, replay_detected };

struct RefreshRotationResult {
  RefreshRotationStatus status;
  std::optional<IssuedRefreshToken> issued;
};

class RefreshTokenService {
public:
  explicit RefreshTokenService(PostgresConnection &connection);
  [[nodiscard]] IssuedRefreshToken create_session(const domain::Uuid &user_id,
                                                  domain::UtcInstant expires_at);
  [[nodiscard]] RefreshRotationResult
  rotate(std::string_view presented_token,
         std::optional<domain::UtcInstant> renewed_expires_at = std::nullopt);
  void logout(const domain::Uuid &session_id);
  void revoke_user_sessions(const domain::Uuid &user_id, std::string_view reason);

  [[nodiscard]] static std::string generate_token();
  [[nodiscard]] static std::string hash_token(std::string_view token);

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
