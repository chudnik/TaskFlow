#include "taskflow/infrastructure/task_cursor.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow;

TEST(TaskCursorTest, RoundTripsNullAndNonNullSortValues) {
  infrastructure::SignedTaskCursorCodec codec{"test-cursor-signing-secret-at-least-32-bytes"};
  const auto id = domain::Uuid::generate();
  const auto null_cursor = codec.decode(codec.encode({1, "query-a", std::nullopt, id}), "query-a");
  EXPECT_FALSE(null_cursor.sort_value);
  EXPECT_EQ(null_cursor.task_id, id);
  const auto valued =
      codec.decode(codec.encode({1, "query-a", "2026-07-27T10:00:00Z", id}), "query-a");
  EXPECT_EQ(valued.sort_value, "2026-07-27T10:00:00Z");
}

TEST(TaskCursorTest, RejectsTamperingAndQueryMismatch) {
  infrastructure::SignedTaskCursorCodec codec{"test-cursor-signing-secret-at-least-32-bytes"};
  auto encoded = codec.encode({1, "query-a", "value", domain::Uuid::generate()});
  encoded.front() = encoded.front() == 'a' ? 'b' : 'a';
  try {
    static_cast<void>(codec.decode(encoded, "query-a"));
    FAIL() << "tampered cursor accepted";
  } catch (const application::CursorError &error) {
    EXPECT_EQ(error.code(), application::CursorErrorCode::invalid_signature);
  }
  const auto valid = codec.encode({1, "query-a", "value", domain::Uuid::generate()});
  try {
    static_cast<void>(codec.decode(valid, "query-b"));
    FAIL() << "mismatched cursor accepted";
  } catch (const application::CursorError &error) {
    EXPECT_EQ(error.code(), application::CursorErrorCode::query_mismatch);
  }
}

} // namespace
