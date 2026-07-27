#include "taskflow/infrastructure/jwt_access_token.hpp"

#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include <chrono>
#include <stdexcept>

namespace taskflow::infrastructure {
namespace {
using JwtClock = std::chrono::system_clock;

struct VerificationClock {
  using time_point = JwtClock::time_point;
  time_point current;
  [[nodiscard]] time_point now() const { return current; }
};

[[nodiscard]] JwtClock::time_point jwt_time(const domain::UtcInstant value) {
  return std::chrono::time_point_cast<JwtClock::duration>(value);
}
} // namespace

JwtAccessTokenService::JwtAccessTokenService(std::string secret, std::string issuer,
                                             std::string audience,
                                             const std::chrono::seconds lifetime,
                                             const domain::Clock &clock,
                                             const application::AccountSessionValidator &validator)
    : secret_{std::move(secret)}, issuer_{std::move(issuer)}, audience_{std::move(audience)},
      lifetime_{lifetime}, clock_{&clock}, validator_{&validator} {
  if (secret_.size() < 32 || issuer_.empty() || audience_.empty() || lifetime_.count() <= 0) {
    throw std::invalid_argument{"invalid access token configuration"};
  }
}

std::string JwtAccessTokenService::create(const domain::User &user,
                                          const domain::Uuid &session_id) const {
  const auto issued_at = jwt_time(clock_->now());
  return jwt::create()
      .set_issuer(issuer_)
      .set_audience(audience_)
      .set_subject(user.id.to_string())
      .set_issued_at(issued_at)
      .set_expires_at(issued_at + lifetime_)
      .set_id(domain::Uuid::generate().to_string())
      .set_payload_claim("session_id", jwt::claim(session_id.to_string()))
      .set_payload_claim(
          "global_role",
          jwt::claim(std::string{user.global_role == domain::GlobalRole::admin ? "admin" : "user"}))
      .sign(jwt::algorithm::hs256{secret_});
}

std::optional<application::AuthenticatedPrincipal>
JwtAccessTokenService::validate(const std::string_view token) const noexcept {
  try {
    const auto decoded = jwt::decode(std::string{token});
    if (decoded.get_algorithm() != "HS256" || !decoded.has_subject() || !decoded.has_expires_at() ||
        !decoded.has_issued_at() || !decoded.has_id() || !decoded.has_payload_claim("session_id") ||
        !decoded.has_payload_claim("global_role")) {
      return std::nullopt;
    }
    std::error_code error;
    const auto now = jwt_time(clock_->now());
    jwt::verify<VerificationClock, jwt::traits::nlohmann_json>(VerificationClock{now})
        .allow_algorithm(jwt::algorithm::hs256{secret_})
        .with_issuer(issuer_)
        .with_audience(audience_)
        .verify(decoded, error);
    if (error) {
      return std::nullopt;
    }
    const auto expiry = decoded.get_expires_at();
    const auto issued = decoded.get_issued_at();
    if (expiry <= now || issued > now || expiry <= issued) {
      return std::nullopt;
    }
    auto user_id = domain::Uuid::parse(decoded.get_subject());
    auto session_id = domain::Uuid::parse(decoded.get_payload_claim("session_id").as_string());
    const auto role = decoded.get_payload_claim("global_role").as_string();
    if (!user_id || !session_id || (role != "user" && role != "admin") ||
        !validator_->account_and_session_active(*user_id, *session_id)) {
      return std::nullopt;
    }
    return application::AuthenticatedPrincipal{
        *user_id, *session_id,
        role == "admin" ? domain::GlobalRole::admin : domain::GlobalRole::user,
        std::chrono::time_point_cast<std::chrono::nanoseconds>(expiry)};
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

} // namespace taskflow::infrastructure
