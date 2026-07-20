#pragma once

#include "taskflow/application/authentication.hpp"
#include "taskflow/domain/common.hpp"

#include <chrono>
#include <string>

namespace taskflow::infrastructure {

class JwtAccessTokenService final : public application::AccessTokenService {
public:
  JwtAccessTokenService(std::string secret, std::string issuer, std::string audience,
                        std::chrono::seconds lifetime, const domain::Clock &clock,
                        const application::AccountSessionValidator &validator);

  [[nodiscard]] std::string create(const domain::User &user,
                                   const domain::Uuid &session_id) const override;
  [[nodiscard]] std::optional<application::AuthenticatedPrincipal>
  validate(std::string_view token) const noexcept override;

private:
  std::string secret_;
  std::string issuer_;
  std::string audience_;
  std::chrono::seconds lifetime_;
  const domain::Clock *clock_;
  const application::AccountSessionValidator *validator_;
};

} // namespace taskflow::infrastructure
