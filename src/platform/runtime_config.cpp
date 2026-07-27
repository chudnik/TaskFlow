#include "taskflow/platform/runtime_config.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

namespace taskflow::platform {
namespace {

[[nodiscard]] std::string strip_line_ending(std::string value) {
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
    value.pop_back();
  }
  return value;
}

[[nodiscard]] std::optional<std::string>
resolve_value(std::string_view name, const RuntimeConfig::EnvironmentLookup &environment,
              const RuntimeConfig::FileReader &read_file) {
  const auto direct = environment(name);
  const auto file_name = std::string{name} + "_FILE";
  const auto file = environment(file_name);

  if (direct && file) {
    throw ConfigError{std::string{name} + " and " + file_name + " cannot both be set"};
  }
  if (direct) {
    return *direct;
  }
  if (!file) {
    return std::nullopt;
  }
  if (file->empty()) {
    throw ConfigError{file_name + " must not be empty"};
  }

  try {
    return strip_line_ending(read_file(*file));
  } catch (const std::exception &) {
    throw ConfigError{"unable to read the file configured by " + file_name};
  }
}

[[nodiscard]] std::string require_value(std::string_view name,
                                        const RuntimeConfig::EnvironmentLookup &environment,
                                        const RuntimeConfig::FileReader &read_file) {
  auto value = resolve_value(name, environment, read_file);
  if (!value || value->empty()) {
    throw ConfigError{std::string{name} + " is required"};
  }
  return std::move(*value);
}

[[nodiscard]] std::string value_or(std::string_view name, std::string fallback,
                                   const RuntimeConfig::EnvironmentLookup &environment,
                                   const RuntimeConfig::FileReader &read_file) {
  auto value = resolve_value(name, environment, read_file);
  if (!value) {
    return fallback;
  }
  if (value->empty()) {
    throw ConfigError{std::string{name} + " must not be empty"};
  }
  return std::move(*value);
}

[[nodiscard]] std::uint16_t parse_port(const std::string &value) {
  unsigned int port = 0;
  const auto *begin = value.data();
  const auto *end = begin + value.size();
  const auto [parsed_until, error] = std::from_chars(begin, end, port);
  if (error != std::errc{} || parsed_until != end || port == 0 || port > 65535) {
    throw ConfigError{"TASKFLOW_HTTP_PORT must be an integer between 1 and 65535"};
  }
  return static_cast<std::uint16_t>(port);
}

[[nodiscard]] std::uint32_t parse_positive(std::string_view name, const std::string &value) {
  std::uint32_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed == 0) {
    throw ConfigError{std::string{name} + " must be a positive integer"};
  }
  return parsed;
}

void validate_log_level(const std::string &level) {
  constexpr std::array<std::string_view, 6> levels{"trace", "debug", "info",
                                                   "warn",  "error", "critical"};
  for (const auto candidate : levels) {
    if (level == candidate) {
      return;
    }
  }
  throw ConfigError{"TASKFLOW_LOG_LEVEL must be one of trace, debug, info, warn, error, critical"};
}

} // namespace

RuntimeConfig RuntimeConfig::from_environment() {
  const EnvironmentLookup environment =
      [](const std::string_view name) -> std::optional<std::string> {
    const auto variable = std::string{name};
    if (const char *value = std::getenv(variable.c_str())) {
      return std::string{value};
    }
    return std::nullopt;
  };
  const FileReader read_file = [](const std::filesystem::path &path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
      throw std::runtime_error{"open failed"};
    }
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  };
  return load(environment, read_file);
}

RuntimeConfig RuntimeConfig::load(const EnvironmentLookup &environment,
                                  const FileReader &read_file) {
  RuntimeConfig config{
      .postgres_dsn = require_value("TASKFLOW_POSTGRES_DSN", environment, read_file),
      .redis_uri = require_value("TASKFLOW_REDIS_URI", environment, read_file),
      .jwt_signing_secret = require_value("TASKFLOW_JWT_SIGNING_SECRET", environment, read_file),
      .jwt_issuer = value_or("TASKFLOW_JWT_ISSUER", "taskflow", environment, read_file),
      .jwt_audience = value_or("TASKFLOW_JWT_AUDIENCE", "taskflow-api", environment, read_file),
      .http_address = value_or("TASKFLOW_HTTP_ADDRESS", "0.0.0.0", environment, read_file),
      .http_port = parse_port(value_or("TASKFLOW_HTTP_PORT", "8080", environment, read_file)),
      .log_level = value_or("TASKFLOW_LOG_LEVEL", "info", environment, read_file),
      .login_rate_limit =
          parse_positive("TASKFLOW_LOGIN_RATE_LIMIT",
                         value_or("TASKFLOW_LOGIN_RATE_LIMIT", "10", environment, read_file)),
      .refresh_rate_limit =
          parse_positive("TASKFLOW_REFRESH_RATE_LIMIT",
                         value_or("TASKFLOW_REFRESH_RATE_LIMIT", "30", environment, read_file)),
      .rate_limit_window_seconds = parse_positive(
          "TASKFLOW_RATE_LIMIT_WINDOW_SECONDS",
          value_or("TASKFLOW_RATE_LIMIT_WINDOW_SECONDS", "60", environment, read_file)),
      .database_timeout_ms =
          parse_positive("TASKFLOW_DATABASE_TIMEOUT_MS",
                         value_or("TASKFLOW_DATABASE_TIMEOUT_MS", "5000", environment, read_file)),
      .http_idle_timeout_seconds = parse_positive(
          "TASKFLOW_HTTP_IDLE_TIMEOUT_SECONDS",
          value_or("TASKFLOW_HTTP_IDLE_TIMEOUT_SECONDS", "30", environment, read_file)),
      .maximum_connections =
          parse_positive("TASKFLOW_MAXIMUM_CONNECTIONS",
                         value_or("TASKFLOW_MAXIMUM_CONNECTIONS", "1000", environment, read_file)),
  };

  if (config.jwt_signing_secret.size() < 32) {
    throw ConfigError{"TASKFLOW_JWT_SIGNING_SECRET must contain at least 32 bytes"};
  }
  validate_log_level(config.log_level);
  return config;
}

std::string RuntimeConfig::redacted_diagnostics() const {
  std::ostringstream output;
  output << "postgres_dsn=<redacted>, redis_uri=<redacted>, jwt_signing_secret=<redacted>"
         << ", jwt_issuer=" << jwt_issuer << ", jwt_audience=" << jwt_audience
         << ", http_address=" << http_address << ", http_port=" << http_port
         << ", log_level=" << log_level;
  output << ", login_rate_limit=" << login_rate_limit
         << ", refresh_rate_limit=" << refresh_rate_limit
         << ", rate_limit_window_seconds=" << rate_limit_window_seconds
         << ", database_timeout_ms=" << database_timeout_ms
         << ", http_idle_timeout_seconds=" << http_idle_timeout_seconds
         << ", maximum_connections=" << maximum_connections;
  return output.str();
}

} // namespace taskflow::platform
