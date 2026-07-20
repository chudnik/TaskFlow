#include "taskflow/transport/http/authentication.hpp"
#include "taskflow/transport/websocket/authentication.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow;

class Tokens final : public application::AccessTokenService {
public:
  std::string create(const domain::User &, const domain::Uuid &) const override { return {}; }
  std::optional<application::AuthenticatedPrincipal>
  validate(std::string_view token) const noexcept override {
    if (token != "valid-token") {
      return std::nullopt;
    }
    return application::AuthenticatedPrincipal{domain::Uuid::generate(), domain::Uuid::generate(),
                                               domain::GlobalRole::user,
                                               domain::SystemClock{}.now()};
  }
};

TEST(AuthenticationMiddleware, HttpAndWebSocketShareStrictBearerRules) {
  Tokens tokens;
  const transport::http::AuthenticationMiddleware http_auth{tokens};
  const transport::websocket::AuthenticationMiddleware websocket_auth{tokens};
  EXPECT_TRUE(http_auth.authenticate_bearer("Bearer valid-token"));
  EXPECT_TRUE(websocket_auth.authenticate_bearer("Bearer valid-token"));
  EXPECT_FALSE(http_auth.authenticate_bearer("bearer valid-token"));
  EXPECT_FALSE(websocket_auth.authenticate_bearer("Bearer valid-token extra"));
  EXPECT_FALSE(http_auth.authenticate_bearer("valid-token"));
}
} // namespace
