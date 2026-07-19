#include "taskflow/domain/common.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace taskflow::domain {
namespace {

TEST(UuidTest, ParsesAndFormatsCanonicalValue) {
  const auto parsed = Uuid::parse("123e4567-e89b-12d3-a456-426614174000");
  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->to_string(), "123e4567-e89b-12d3-a456-426614174000");
  EXPECT_FALSE(Uuid::parse("123e4567e89b12d3a456426614174000"));
  EXPECT_FALSE(Uuid::parse("123e4567-e89b-12d3-a456-42661417400z"));
}

TEST(UuidTest, GeneratesVersionFourVariantOneValue) {
  const auto generated = Uuid::generate();
  EXPECT_EQ(generated.bytes()[6] >> 4U, 4U);
  EXPECT_EQ(generated.bytes()[8] >> 6U, 2U);
  EXPECT_EQ(Uuid::parse(generated.to_string()), generated);
}

TEST(UtcTimeTest, StrictlyRoundTripsRfc3339Utc) {
  const auto parsed = parse_utc("2026-07-19T12:34:56.123456789Z");
  ASSERT_TRUE(parsed);
  EXPECT_EQ(format_utc(*parsed), "2026-07-19T12:34:56.123456789Z");
  EXPECT_FALSE(parse_utc("2026-02-29T12:34:56Z"));
  EXPECT_FALSE(parse_utc("2026-07-19T12:34:56+03:00"));
}

TEST(ClockTest, FixedClockCanAdvanceDeterministically) {
  const auto initial = parse_utc("2026-07-19T12:00:00Z");
  ASSERT_TRUE(initial);
  FixedClock clock{*initial};
  clock.advance(std::chrono::minutes{5});
  EXPECT_EQ(format_utc(clock.now()), "2026-07-19T12:05:00Z");
}

TEST(PageRequestTest, AppliesBoundsAndCursorRules) {
  const auto defaults = PageRequest::create(std::nullopt);
  ASSERT_TRUE(defaults);
  EXPECT_EQ(defaults->size, 25U);
  EXPECT_TRUE(PageRequest::create(100, "cursor"));
  EXPECT_FALSE(PageRequest::create(0));
  EXPECT_FALSE(PageRequest::create(101));
  EXPECT_FALSE(PageRequest::create(25, ""));
}

TEST(ValidationErrorsTest, CountsUnicodeAndRejectsInvalidText) {
  ValidationErrors errors;
  errors.require_text("title", "задача", 1, 6);
  EXPECT_TRUE(errors.empty());
  errors.require_text("blank", " \t", 1, 10);
  errors.require_text("long", "семьбукв", 1, 7);
  const std::string invalid_utf8{"\xC0\xAF", 2};
  errors.require_text("encoding", invalid_utf8, 1, 10);
  ASSERT_EQ(errors.items().size(), 3U);
  EXPECT_EQ(errors.items()[0].code, "too_short");
  EXPECT_EQ(errors.items()[1].code, "too_long");
  EXPECT_EQ(errors.items()[2].code, "invalid_utf8");
}

} // namespace
} // namespace taskflow::domain
