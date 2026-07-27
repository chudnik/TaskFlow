#include "taskflow/platform/runtime_config.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace {

using taskflow::platform::ConfigError;
using taskflow::platform::RuntimeConfig;

[[nodiscard]] RuntimeConfig::EnvironmentLookup
environment(std::unordered_map<std::string, std::string> values) {
  return [values = std::move(values)](const std::string_view name) -> std::optional<std::string> {
    const auto item = values.find(std::string{name});
    if (item == values.end()) {
      return std::nullopt;
    }
    return item->second;
  };
}

const RuntimeConfig::FileReader no_files = [](const auto &) -> std::string {
  throw std::runtime_error{"unexpected file read"};
};

TEST(RuntimeConfigTest, LoadsValidatedValuesAndDefaults) {
  const auto config = RuntimeConfig::load(
      environment({{"TASKFLOW_POSTGRES_DSN", "postgresql://db/taskflow"},
                   {"TASKFLOW_REDIS_URI", "redis://redis:6379"},
                   {"TASKFLOW_JWT_SIGNING_SECRET", "0123456789abcdef0123456789abcdef"}}),
      no_files);

  EXPECT_EQ(config.http_address, "0.0.0.0");
  EXPECT_EQ(config.http_port, 8080);
  EXPECT_EQ(config.jwt_issuer, "taskflow");
  EXPECT_EQ(config.log_level, "info");
  EXPECT_EQ(config.login_rate_limit, 10U);
  EXPECT_EQ(config.refresh_rate_limit, 30U);
  EXPECT_EQ(config.rate_limit_window_seconds, 60U);
  EXPECT_EQ(config.worker_poll_interval_ms, 500U);
  EXPECT_EQ(config.worker_batch_size, 16U);
  EXPECT_EQ(config.worker_lease_seconds, 30U);
  EXPECT_EQ(config.worker_retry_initial_ms, 250U);
  EXPECT_EQ(config.worker_retry_max_ms, 30000U);
  EXPECT_EQ(config.shutdown_timeout_seconds, 30U);
}

TEST(RuntimeConfigTest, LoadsBoundedWorkerAndShutdownSettings) {
  const auto config = RuntimeConfig::load(
      environment({{"TASKFLOW_POSTGRES_DSN", "postgresql://db/taskflow"},
                   {"TASKFLOW_REDIS_URI", "redis://redis:6379"},
                   {"TASKFLOW_JWT_SIGNING_SECRET", "0123456789abcdef0123456789abcdef"},
                   {"TASKFLOW_WORKER_POLL_INTERVAL_MS", "100"},
                   {"TASKFLOW_WORKER_BATCH_SIZE", "64"},
                   {"TASKFLOW_WORKER_LEASE_SECONDS", "45"},
                   {"TASKFLOW_WORKER_RETRY_INITIAL_MS", "50"},
                   {"TASKFLOW_WORKER_RETRY_MAX_MS", "5000"},
                   {"TASKFLOW_SHUTDOWN_TIMEOUT_SECONDS", "20"}}),
      no_files);

  EXPECT_EQ(config.worker_poll_interval_ms, 100U);
  EXPECT_EQ(config.worker_batch_size, 64U);
  EXPECT_EQ(config.worker_lease_seconds, 45U);
  EXPECT_EQ(config.worker_retry_initial_ms, 50U);
  EXPECT_EQ(config.worker_retry_max_ms, 5000U);
  EXPECT_EQ(config.shutdown_timeout_seconds, 20U);
}

TEST(RuntimeConfigTest, ReadsSecretFromMountedFileAndStripsLineEnding) {
  const auto config =
      RuntimeConfig::load(environment({{"TASKFLOW_POSTGRES_DSN", "postgresql://db/taskflow"},
                                       {"TASKFLOW_REDIS_URI", "redis://redis:6379"},
                                       {"TASKFLOW_JWT_SIGNING_SECRET_FILE", "/run/secrets/jwt"}}),
                          [](const auto &path) {
                            EXPECT_EQ(path, "/run/secrets/jwt");
                            return std::string{"abcdefghijklmnopqrstuvwxyz123456\r\n"};
                          });

  EXPECT_EQ(config.jwt_signing_secret, "abcdefghijklmnopqrstuvwxyz123456");
}

TEST(RuntimeConfigTest, RejectsMissingSigningSecretWithoutLeakingOtherValues) {
  try {
    static_cast<void>(RuntimeConfig::load(
        environment({{"TASKFLOW_POSTGRES_DSN", "postgresql://user:password@db/taskflow"},
                     {"TASKFLOW_REDIS_URI", "redis://:password@redis:6379"}}),
        no_files));
    FAIL() << "expected ConfigError";
  } catch (const ConfigError &error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("TASKFLOW_JWT_SIGNING_SECRET"), std::string::npos);
    EXPECT_EQ(message.find("password"), std::string::npos);
  }
}

TEST(RuntimeConfigTest, RejectsDirectAndFileValueTogether) {
  EXPECT_THROW(static_cast<void>(RuntimeConfig::load(
                   environment({{"TASKFLOW_POSTGRES_DSN", "postgresql://db/taskflow"},
                                {"TASKFLOW_REDIS_URI", "redis://redis:6379"},
                                {"TASKFLOW_JWT_SIGNING_SECRET", "0123456789abcdef0123456789abcdef"},
                                {"TASKFLOW_JWT_SIGNING_SECRET_FILE", "/run/secrets/jwt"}}),
                   no_files)),
               ConfigError);
}

TEST(RuntimeConfigTest, RejectsInvalidPortAndLogLevel) {
  const auto base = std::unordered_map<std::string, std::string>{
      {"TASKFLOW_POSTGRES_DSN", "postgresql://db/taskflow"},
      {"TASKFLOW_REDIS_URI", "redis://redis:6379"},
      {"TASKFLOW_JWT_SIGNING_SECRET", "0123456789abcdef0123456789abcdef"}};

  auto invalid_port = base;
  invalid_port["TASKFLOW_HTTP_PORT"] = "70000";
  EXPECT_THROW(
      static_cast<void>(RuntimeConfig::load(environment(std::move(invalid_port)), no_files)),
      ConfigError);

  auto invalid_level = base;
  invalid_level["TASKFLOW_LOG_LEVEL"] = "verbose";
  EXPECT_THROW(
      static_cast<void>(RuntimeConfig::load(environment(std::move(invalid_level)), no_files)),
      ConfigError);
}

TEST(RuntimeConfigTest, RedactsCredentialsInDiagnostics) {
  const auto config = RuntimeConfig::load(
      environment({{"TASKFLOW_POSTGRES_DSN", "postgresql://user:db-secret@db/taskflow"},
                   {"TASKFLOW_REDIS_URI", "redis://:redis-secret@redis:6379"},
                   {"TASKFLOW_JWT_SIGNING_SECRET", "jwt-secret-0123456789abcdef012345"}}),
      no_files);

  const auto diagnostics = config.redacted_diagnostics();
  EXPECT_EQ(diagnostics.find("db-secret"), std::string::npos);
  EXPECT_EQ(diagnostics.find("redis-secret"), std::string::npos);
  EXPECT_EQ(diagnostics.find("jwt-secret"), std::string::npos);
  EXPECT_NE(diagnostics.find("<redacted>"), std::string::npos);
}

TEST(RuntimeConfigTest, RejectsZeroOperationalLimit) {
  EXPECT_THROW(static_cast<void>(RuntimeConfig::load(
                   environment({{"TASKFLOW_POSTGRES_DSN", "postgresql://db/taskflow"},
                                {"TASKFLOW_REDIS_URI", "redis://redis:6379"},
                                {"TASKFLOW_JWT_SIGNING_SECRET", "0123456789abcdef0123456789abcdef"},
                                {"TASKFLOW_MAXIMUM_CONNECTIONS", "0"}}),
                   no_files)),
               ConfigError);
}

TEST(RuntimeConfigTest, RejectsUnsafeWorkerBoundsAndRetryOrdering) {
  const auto base = std::unordered_map<std::string, std::string>{
      {"TASKFLOW_POSTGRES_DSN", "postgresql://db/taskflow"},
      {"TASKFLOW_REDIS_URI", "redis://redis:6379"},
      {"TASKFLOW_JWT_SIGNING_SECRET", "0123456789abcdef0123456789abcdef"}};

  auto oversized_batch = base;
  oversized_batch["TASKFLOW_WORKER_BATCH_SIZE"] = "1001";
  EXPECT_THROW(
      static_cast<void>(RuntimeConfig::load(environment(std::move(oversized_batch)), no_files)),
      ConfigError);

  auto invalid_retry = base;
  invalid_retry["TASKFLOW_WORKER_RETRY_INITIAL_MS"] = "5000";
  invalid_retry["TASKFLOW_WORKER_RETRY_MAX_MS"] = "100";
  EXPECT_THROW(
      static_cast<void>(RuntimeConfig::load(environment(std::move(invalid_retry)), no_files)),
      ConfigError);

  auto excessive_shutdown = base;
  excessive_shutdown["TASKFLOW_SHUTDOWN_TIMEOUT_SECONDS"] = "301";
  EXPECT_THROW(
      static_cast<void>(RuntimeConfig::load(environment(std::move(excessive_shutdown)), no_files)),
      ConfigError);
}

} // namespace
