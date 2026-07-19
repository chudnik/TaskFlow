#pragma once

#include <cstdint>
#include <string>

namespace taskflow::infrastructure {

inline constexpr std::int64_t expected_schema_version = 1;

enum class SchemaCompatibility {
  compatible,
  metadata_missing,
  migration_in_progress,
  version_too_old,
  version_too_new,
};

struct SchemaCompatibilityResult {
  SchemaCompatibility status;
  std::int64_t current_version;

  [[nodiscard]] bool is_compatible() const noexcept;
  [[nodiscard]] std::string message() const;
};

[[nodiscard]] SchemaCompatibilityResult evaluate_schema_compatibility(
    bool metadata_exists, bool migration_in_progress, std::int64_t current_version,
    std::int64_t required_version = expected_schema_version);

[[nodiscard]] SchemaCompatibilityResult check_postgres_schema(const std::string &dsn);

} // namespace taskflow::infrastructure
