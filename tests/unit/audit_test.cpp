#include "taskflow/application/audit.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow;

TEST(AuditTest, RedactsSensitiveFieldsWithoutDroppingBusinessContext) {
  const auto sanitized = application::sanitize_audit_fields(
      {{"title", "Visible"}, {"refresh_token", "secret"}, {"password_hash", "hash"}});
  EXPECT_EQ(sanitized.at("title"), "Visible");
  EXPECT_EQ(sanitized.at("refresh_token"), "[REDACTED]");
  EXPECT_EQ(sanitized.at("password_hash"), "[REDACTED]");
}

} // namespace
