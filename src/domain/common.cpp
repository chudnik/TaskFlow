#include "taskflow/domain/common.hpp"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <random>
#include <sstream>

namespace taskflow::domain {
namespace {

[[nodiscard]] int hex_value(const char value) noexcept {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

[[nodiscard]] bool parse_number(const std::string_view value, int &output) noexcept {
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), output);
  return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

[[nodiscard]] std::optional<std::size_t> utf8_code_points(const std::string_view value) noexcept {
  std::size_t count = 0;
  for (std::size_t index = 0; index < value.size();) {
    const auto lead = static_cast<unsigned char>(value[index]);
    std::size_t width = 0;
    std::uint32_t code_point = 0;
    if (lead <= 0x7F) {
      width = 1;
      code_point = lead;
    } else if ((lead & 0xE0U) == 0xC0U) {
      width = 2;
      code_point = lead & 0x1FU;
    } else if ((lead & 0xF0U) == 0xE0U) {
      width = 3;
      code_point = lead & 0x0FU;
    } else if ((lead & 0xF8U) == 0xF0U) {
      width = 4;
      code_point = lead & 0x07U;
    } else {
      return std::nullopt;
    }
    if (index + width > value.size()) {
      return std::nullopt;
    }
    for (std::size_t offset = 1; offset < width; ++offset) {
      const auto continuation = static_cast<unsigned char>(value[index + offset]);
      if ((continuation & 0xC0U) != 0x80U) {
        return std::nullopt;
      }
      code_point = (code_point << 6U) | (continuation & 0x3FU);
    }
    const bool overlong = (width == 2 && code_point < 0x80U) ||
                          (width == 3 && code_point < 0x800U) ||
                          (width == 4 && code_point < 0x10000U);
    if (overlong || code_point > 0x10FFFFU ||
        (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
      return std::nullopt;
    }
    index += width;
    ++count;
  }
  return count;
}

[[nodiscard]] bool is_blank(const std::string_view value) noexcept {
  return std::all_of(value.begin(), value.end(), [](const char character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
  });
}

} // namespace

Uuid::Uuid(std::array<std::uint8_t, 16> bytes) : bytes_{bytes} {}

Uuid Uuid::generate() {
  std::random_device random;
  std::array<std::uint8_t, 16> bytes{};
  for (auto &byte : bytes) {
    byte = static_cast<std::uint8_t>(random());
  }
  bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0FU) | 0x40U);
  bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3FU) | 0x80U);
  return Uuid{bytes};
}

std::optional<Uuid> Uuid::parse(const std::string_view value) noexcept {
  if (value.size() != 36 || value[8] != '-' || value[13] != '-' || value[18] != '-' ||
      value[23] != '-') {
    return std::nullopt;
  }
  std::array<std::uint8_t, 16> bytes{};
  std::size_t source = 0;
  for (std::size_t target = 0; target < bytes.size(); ++target) {
    if (source == 8 || source == 13 || source == 18 || source == 23) {
      ++source;
    }
    const int high = hex_value(value[source]);
    const int low = hex_value(value[source + 1]);
    if (high < 0 || low < 0) {
      return std::nullopt;
    }
    bytes[target] = static_cast<std::uint8_t>((high << 4) | low);
    source += 2;
  }
  return Uuid{bytes};
}

std::string Uuid::to_string() const {
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(36);
  for (std::size_t index = 0; index < bytes_.size(); ++index) {
    if (index == 4 || index == 6 || index == 8 || index == 10) {
      result.push_back('-');
    }
    result.push_back(digits[bytes_[index] >> 4U]);
    result.push_back(digits[bytes_[index] & 0x0FU]);
  }
  return result;
}

const std::array<std::uint8_t, 16> &Uuid::bytes() const noexcept { return bytes_; }

std::string format_utc(const UtcInstant instant) {
  using namespace std::chrono;
  const auto seconds_part = floor<seconds>(instant);
  const auto days_part = floor<days>(seconds_part);
  const year_month_day date{days_part};
  const hh_mm_ss time{seconds_part - days_part};
  const auto nanoseconds_part = duration_cast<nanoseconds>(instant - seconds_part).count();

  std::ostringstream output;
  output << std::setfill('0') << std::setw(4) << static_cast<int>(date.year()) << '-'
         << std::setw(2) << static_cast<unsigned>(date.month()) << '-' << std::setw(2)
         << static_cast<unsigned>(date.day()) << 'T' << std::setw(2) << time.hours().count() << ':'
         << std::setw(2) << time.minutes().count() << ':' << std::setw(2) << time.seconds().count();
  if (nanoseconds_part != 0) {
    output << '.' << std::setw(9) << nanoseconds_part;
    auto formatted = output.str();
    while (formatted.back() == '0') {
      formatted.pop_back();
    }
    return formatted + 'Z';
  }
  output << 'Z';
  return output.str();
}

std::optional<UtcInstant> parse_utc(const std::string_view value) noexcept {
  using namespace std::chrono;
  if (value.size() < 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
      value[13] != ':' || value[16] != ':' || value.back() != 'Z') {
    return std::nullopt;
  }
  int year_value = 0;
  int month_value = 0;
  int day_value = 0;
  int hour_value = 0;
  int minute_value = 0;
  int second_value = 0;
  if (!parse_number(value.substr(0, 4), year_value) ||
      !parse_number(value.substr(5, 2), month_value) ||
      !parse_number(value.substr(8, 2), day_value) ||
      !parse_number(value.substr(11, 2), hour_value) ||
      !parse_number(value.substr(14, 2), minute_value) ||
      !parse_number(value.substr(17, 2), second_value)) {
    return std::nullopt;
  }
  const year_month_day date{year{year_value}, month{static_cast<unsigned>(month_value)},
                            day{static_cast<unsigned>(day_value)}};
  if (!date.ok() || hour_value > 23 || minute_value > 59 || second_value > 59) {
    return std::nullopt;
  }

  nanoseconds fractional{0};
  if (value.size() > 20) {
    if (value[19] != '.' || value.size() > 30 || value.size() == 21) {
      return std::nullopt;
    }
    const auto digits = value.substr(20, value.size() - 21);
    std::uint32_t fraction_value = 0;
    const auto parsed =
        std::from_chars(digits.data(), digits.data() + digits.size(), fraction_value);
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) {
      return std::nullopt;
    }
    for (std::size_t index = digits.size(); index < 9; ++index) {
      fraction_value *= 10U;
    }
    fractional = nanoseconds{fraction_value};
  }
  return UtcInstant{sys_days{date}.time_since_epoch() + hours{hour_value} +
                    minutes{minute_value} + seconds{second_value} + fractional};
}

UtcInstant SystemClock::now() const {
  return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
}

FixedClock::FixedClock(const UtcInstant instant) : instant_{instant} {}

UtcInstant FixedClock::now() const { return instant_; }

void FixedClock::set(const UtcInstant instant) { instant_ = instant; }

void FixedClock::advance(const std::chrono::nanoseconds duration) { instant_ += duration; }

std::optional<PageRequest> PageRequest::create(const std::optional<std::size_t> requested_size,
                                               std::optional<std::string> cursor) {
  const std::size_t size = requested_size.value_or(default_size);
  if (size == 0 || size > maximum_size || (cursor && cursor->empty())) {
    return std::nullopt;
  }
  return PageRequest{size, std::move(cursor)};
}

void ValidationErrors::add(std::string field, std::string code, std::string message) {
  errors_.push_back(
      ValidationError{std::move(field), std::move(code), std::move(message)});
}

void ValidationErrors::require_text(const std::string_view field, const std::string_view value,
                                    const std::size_t minimum_length,
                                    const std::size_t maximum_length) {
  const auto length = utf8_code_points(value);
  if (!length) {
    add(std::string{field}, "invalid_utf8", "must be valid UTF-8");
  } else if (*length < minimum_length || is_blank(value)) {
    add(std::string{field}, "too_short", "must not be blank and is shorter than allowed");
  } else if (*length > maximum_length) {
    add(std::string{field}, "too_long", "is longer than allowed");
  }
}

bool ValidationErrors::empty() const noexcept { return errors_.empty(); }

const std::vector<ValidationError> &ValidationErrors::items() const noexcept { return errors_; }

} // namespace taskflow::domain
