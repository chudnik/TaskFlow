#pragma once

#include <array>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace taskflow::domain {

class Uuid {
public:
  [[nodiscard]] static Uuid generate();
  [[nodiscard]] static std::optional<Uuid> parse(std::string_view value) noexcept;

  [[nodiscard]] std::string to_string() const;
  [[nodiscard]] const std::array<std::uint8_t, 16> &bytes() const noexcept;
  auto operator<=>(const Uuid &) const = default;

private:
  explicit Uuid(std::array<std::uint8_t, 16> bytes);
  std::array<std::uint8_t, 16> bytes_;
};

using UtcInstant = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

[[nodiscard]] std::string format_utc(UtcInstant instant);
[[nodiscard]] std::optional<UtcInstant> parse_utc(std::string_view value) noexcept;

class Clock {
public:
  virtual ~Clock() = default;
  [[nodiscard]] virtual UtcInstant now() const = 0;
};

class SystemClock final : public Clock {
public:
  [[nodiscard]] UtcInstant now() const override;
};

class FixedClock final : public Clock {
public:
  explicit FixedClock(UtcInstant instant);
  [[nodiscard]] UtcInstant now() const override;
  void set(UtcInstant instant);
  void advance(std::chrono::nanoseconds duration);

private:
  UtcInstant instant_;
};

struct PageRequest {
  static constexpr std::size_t default_size = 25;
  static constexpr std::size_t maximum_size = 100;

  std::size_t size;
  std::optional<std::string> cursor;

  [[nodiscard]] static std::optional<PageRequest> create(std::optional<std::size_t> requested_size,
                                                         std::optional<std::string> cursor = {});
};

struct ValidationError {
  std::string field;
  std::string code;
  std::string message;
  auto operator<=>(const ValidationError &) const = default;
};

class ValidationErrors {
public:
  void add(std::string field, std::string code, std::string message);
  void require_text(std::string_view field, std::string_view value, std::size_t minimum_length,
                    std::size_t maximum_length);

  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] const std::vector<ValidationError> &items() const noexcept;

private:
  std::vector<ValidationError> errors_;
};

} // namespace taskflow::domain
