#include "taskflow/infrastructure/schema_compatibility.hpp"

#include <gtest/gtest.h>

namespace taskflow::infrastructure {
namespace {

TEST(SchemaCompatibilityTest, AcceptsExactVersion) {
  const auto result = evaluate_schema_compatibility(true, false, 7, 7);
  EXPECT_TRUE(result.is_compatible());
}

TEST(SchemaCompatibilityTest, RejectsMissingMetadata) {
  const auto result = evaluate_schema_compatibility(false, false, 0, 0);
  EXPECT_EQ(result.status, SchemaCompatibility::metadata_missing);
}

TEST(SchemaCompatibilityTest, RejectsMigrationInProgress) {
  const auto result = evaluate_schema_compatibility(true, true, 7, 7);
  EXPECT_EQ(result.status, SchemaCompatibility::migration_in_progress);
}

TEST(SchemaCompatibilityTest, RejectsOlderAndNewerVersions) {
  EXPECT_EQ(evaluate_schema_compatibility(true, false, 6, 7).status,
            SchemaCompatibility::version_too_old);
  EXPECT_EQ(evaluate_schema_compatibility(true, false, 8, 7).status,
            SchemaCompatibility::version_too_new);
}

} // namespace
} // namespace taskflow::infrastructure
