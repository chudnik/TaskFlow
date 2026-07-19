#include "taskflow/transport/http/api_router.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace taskflow::transport::http {
namespace {

TEST(ApiRouterTest, SerializesStableJsonErrorEnvelope) {
  const ApiError error{422,
                       "validation_failed",
                       "request contains invalid fields",
                       {{"title", "too_short", "must not be blank"}},
                       "request-123"};
  const auto document = nlohmann::json::parse(serialize_error(error));
  EXPECT_EQ(document["error"]["code"], "validation_failed");
  EXPECT_EQ(document["error"]["request_id"], "request-123");
  ASSERT_EQ(document["error"]["details"].size(), 1U);
  EXPECT_EQ(document["error"]["details"][0]["field"], "title");
}

TEST(ApiRouterTest, PreservesSafeRequestIdAndGeneratesUnsafeOne) {
  const auto supplied = validate_request("GET", 0, "", "client.request-123");
  EXPECT_EQ(supplied.request_id, "client.request-123");
  const auto generated = validate_request("GET", 0, "", "contains spaces");
  EXPECT_NE(generated.request_id, "contains spaces");
  EXPECT_EQ(generated.request_id.size(), 36U);
}

TEST(ApiRouterTest, RejectsOversizedBodies) {
  const auto validation =
      validate_request("POST", maximum_request_body_bytes + 1, "application/json", "req-1");
  ASSERT_TRUE(validation.error);
  EXPECT_EQ(validation.error->status, 413);
  EXPECT_EQ(validation.error->code, "request_too_large");
}

TEST(ApiRouterTest, EnforcesJsonForMutationBodies) {
  EXPECT_TRUE(is_json_content_type("Application/JSON; charset=utf-8"));
  const auto rejected = validate_request("PATCH", 2, "text/plain", "req-2");
  ASSERT_TRUE(rejected.error);
  EXPECT_EQ(rejected.error->status, 415);
  EXPECT_FALSE(validate_request("POST", 2, "application/json", "req-3").error);
  EXPECT_FALSE(validate_request("GET", 0, "", "req-4").error);
}

TEST(ApiRouterTest, SerializesHealthContracts) {
  EXPECT_EQ(nlohmann::json::parse(serialize_liveness())["status"], "alive");
  const auto ready = nlohmann::json::parse(
      serialize_readiness(ReadinessReport{true, "available", "compatible"}));
  EXPECT_EQ(ready["status"], "ready");
  EXPECT_EQ(ready["checks"]["schema"], "compatible");
  const auto unavailable = nlohmann::json::parse(
      serialize_readiness(ReadinessReport{false, "unavailable", "unknown"}));
  EXPECT_EQ(unavailable["status"], "unavailable");
}

} // namespace
} // namespace taskflow::transport::http
