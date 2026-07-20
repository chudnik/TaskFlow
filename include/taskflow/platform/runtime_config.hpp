#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace taskflow::platform {

class ConfigError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

struct RuntimeConfig {
  using EnvironmentLookup = std::function<std::optional<std::string>(std::string_view)>;
  using FileReader = std::function<std::string(const std::filesystem::path &)>;

  std::string postgres_dsn;
  std::string redis_uri;
  std::string jwt_signing_secret;
  std::string jwt_issuer;
  std::string jwt_audience;
  std::string http_address;
  std::uint16_t http_port;
  std::string log_level;
  std::uint32_t login_rate_limit;
  std::uint32_t refresh_rate_limit;
  std::uint32_t rate_limit_window_seconds;

  [[nodiscard]] static RuntimeConfig from_environment();
  [[nodiscard]] static RuntimeConfig load(const EnvironmentLookup &environment,
                                          const FileReader &read_file);
  [[nodiscard]] std::string redacted_diagnostics() const;
};

} // namespace taskflow::platform
