#include "taskflow/transport/websocket/protocol.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {
using namespace taskflow;

TEST(WebSocketProtocolTest, ParsesMonotonicControlFrames) {
  const auto ack =
      transport::websocket::parse_client_control(R"({"v":1,"kind":"ack","sequence_id":42})");
  ASSERT_TRUE(ack);
  EXPECT_EQ(ack->kind, transport::websocket::ClientControl::Kind::acknowledge);
  EXPECT_EQ(ack->sequence_id, 42U);
  const auto resume = transport::websocket::parse_client_control(
      R"({"v":1,"kind":"resume","after_sequence_id":41})");
  ASSERT_TRUE(resume);
  EXPECT_EQ(resume->kind, transport::websocket::ClientControl::Kind::resume);
  EXPECT_FALSE(
      transport::websocket::parse_client_control(R"({"v":2,"kind":"ack","sequence_id":42})"));
  EXPECT_EQ(nlohmann::json::parse(transport::websocket::serialize_control(
                "resync_required", 50, "retention_expired"))["kind"],
            "resync_required");
}

} // namespace
