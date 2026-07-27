#include "taskflow/transport/websocket/gateway.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow;

class Tokens final : public application::AccessTokenService {
public:
  std::optional<application::AuthenticatedPrincipal> principal;
  std::string create(const domain::User &, const domain::Uuid &) const override { return {}; }
  std::optional<application::AuthenticatedPrincipal>
  validate(std::string_view) const noexcept override {
    return principal;
  }
};

TEST(WebSocketGatewayTest, AuthenticatesBoundsExpiresAndCleansConnections) {
  const auto now = *domain::parse_utc("2026-07-27T10:00:00Z");
  domain::FixedClock clock{now};
  Tokens tokens;
  tokens.principal =
      application::AuthenticatedPrincipal{domain::Uuid::generate(), domain::Uuid::generate(),
                                          domain::GlobalRole::user, now + std::chrono::seconds{10}};
  application::AuthenticationMiddleware auth{tokens};
  transport::websocket::Gateway gateway{auth, clock, 1, std::chrono::seconds{5}};
  EXPECT_FALSE(gateway.open("invalid"));
  const auto id = gateway.open("Bearer token");
  ASSERT_TRUE(id);
  const auto project = domain::Uuid::generate();
  gateway.authorize_project(*id, project);
  EXPECT_TRUE(gateway.can_deliver(*id, project));
  gateway.membership_removed(tokens.principal->user_id, project);
  EXPECT_FALSE(gateway.can_deliver(*id, project));
  EXPECT_TRUE(gateway.enqueue(*id, "{}"));
  EXPECT_FALSE(gateway.enqueue(*id, "{}"));
  EXPECT_EQ(gateway.find(*id)->close_reason, transport::websocket::CloseReason::slow_consumer);
  gateway.cleanup(*id);
  EXPECT_EQ(gateway.size(), 0U);

  const auto expiring = gateway.open("Bearer token");
  clock.advance(std::chrono::seconds{11});
  gateway.sweep();
  EXPECT_EQ(gateway.find(*expiring)->close_reason,
            transport::websocket::CloseReason::token_expired);
}

} // namespace
