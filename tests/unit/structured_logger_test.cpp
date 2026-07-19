#include "taskflow/platform/structured_logger.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <stdexcept>

namespace {

using taskflow::platform::CorrelationContext;
using taskflow::platform::FieldSensitivity;
using taskflow::platform::format_json_log;
using taskflow::platform::LogRecord;

TEST(StructuredLoggerTest, FormatsRequestCompletionAsJson) {
  const auto encoded = format_json_log(
      LogRecord{.timestamp = "2026-07-19T10:11:12.123Z",
                .level = "info",
                .service = "taskflow-api",
                .correlation = CorrelationContext::request("request-123", "/api/v1/tasks"),
                .outcome = "success",
                .latency_ms = 42,
                .message = "request completed",
                .fields = {{"status", "200", FieldSensitivity::public_value}}});

  const auto json = nlohmann::json::parse(encoded);
  EXPECT_EQ(json["timestamp"], "2026-07-19T10:11:12.123Z");
  EXPECT_EQ(json["correlation_id"], "request-123");
  EXPECT_EQ(json["route"], "/api/v1/tasks");
  EXPECT_EQ(json["outcome"], "success");
  EXPECT_EQ(json["latency_ms"], 42);
  EXPECT_EQ(json["fields"]["status"], "200");
  EXPECT_FALSE(json.contains("job_type"));
}

TEST(StructuredLoggerTest, CarriesJobCorrelationContext) {
  const auto encoded = format_json_log(
      LogRecord{.timestamp = "2026-07-19T10:11:12.123Z",
                .level = "error",
                .service = "taskflow-worker",
                .correlation = CorrelationContext::job("job-456", "deadline-reminder"),
                .outcome = "retry",
                .latency_ms = 15,
                .message = "job failed",
                .fields = {}});

  const auto json = nlohmann::json::parse(encoded);
  EXPECT_EQ(json["correlation_id"], "job-456");
  EXPECT_EQ(json["job_type"], "deadline-reminder");
  EXPECT_FALSE(json.contains("route"));
}

TEST(StructuredLoggerTest, RedactsSecretsAndOmitsPersonalData) {
  constexpr auto password = "password-value-must-not-leak";
  constexpr auto token = "token-value-must-not-leak";
  constexpr auto email = "private@example.test";
  const auto encoded =
      format_json_log(LogRecord{.timestamp = "2026-07-19T10:11:12.123Z",
                                .level = "warn",
                                .service = "taskflow-api",
                                .correlation = CorrelationContext::request("request-789", "/login"),
                                .outcome = "rejected",
                                .latency_ms = 3,
                                .message = "login rejected",
                                .fields = {{"password", password, FieldSensitivity::secret},
                                           {"access_token", token, FieldSensitivity::secret},
                                           {"email", email, FieldSensitivity::personal_data}}});

  EXPECT_EQ(encoded.find(password), std::string::npos);
  EXPECT_EQ(encoded.find(token), std::string::npos);
  EXPECT_EQ(encoded.find(email), std::string::npos);
  const auto json = nlohmann::json::parse(encoded);
  EXPECT_EQ(json["fields"]["password"], "<redacted>");
  EXPECT_EQ(json["fields"]["access_token"], "<redacted>");
  EXPECT_FALSE(json["fields"].contains("email"));
}

TEST(StructuredLoggerTest, RejectsIncompleteCorrelationContext) {
  EXPECT_THROW(static_cast<void>(CorrelationContext::request("", "/tasks")), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(CorrelationContext::job("job-1", "")), std::invalid_argument);
}

} // namespace
