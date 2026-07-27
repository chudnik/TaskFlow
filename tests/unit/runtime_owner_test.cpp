#include "taskflow/service/runtime.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

namespace {

using taskflow::platform::LifecycleState;
using taskflow::platform::RuntimeConfig;
using taskflow::service::ApiRuntimeOwner;
using taskflow::service::DependencyConstructionError;
using taskflow::service::WorkerRuntimeOwner;

[[nodiscard]] RuntimeConfig config() {
  return RuntimeConfig{
      .postgres_dsn = "postgresql://db/taskflow",
      .redis_uri = "redis://redis:6379",
      .jwt_signing_secret = "0123456789abcdef0123456789abcdef",
      .jwt_issuer = "taskflow",
      .jwt_audience = "taskflow-api",
      .http_address = "0.0.0.0",
      .http_port = 8080,
      .log_level = "info",
      .login_rate_limit = 10,
      .refresh_rate_limit = 30,
      .rate_limit_window_seconds = 60,
      .database_timeout_ms = 5000,
      .http_idle_timeout_seconds = 30,
      .maximum_connections = 1000,
      .worker_poll_interval_ms = 500,
      .worker_batch_size = 16,
      .worker_lease_seconds = 30,
      .worker_retry_initial_ms = 250,
      .worker_retry_max_ms = 30000,
      .shutdown_timeout_seconds = 30,
  };
}

struct TrackedDependency {
  TrackedDependency(std::vector<int> &destroyed_values, const int dependency_id)
      : destroyed{&destroyed_values}, id{dependency_id} {}
  ~TrackedDependency() { destroyed->push_back(id); }
  TrackedDependency(const TrackedDependency &) = delete;
  TrackedDependency &operator=(const TrackedDependency &) = delete;

  std::vector<int> *destroyed;
  int id;
};

struct FailingDependency {
  explicit FailingDependency(const std::string &secret) {
    throw std::runtime_error{"connection rejected: " + secret};
  }
};

TEST(RuntimeOwnerTest, ApiAndWorkerOwnIndependentLifecycles) {
  ApiRuntimeOwner api{config()};
  WorkerRuntimeOwner worker{config()};

  EXPECT_TRUE(api.runtime().start());
  EXPECT_TRUE(worker.runtime().start());
  EXPECT_EQ(api.runtime().lifecycle().state(), LifecycleState::running);
  EXPECT_EQ(worker.runtime().lifecycle().state(), LifecycleState::running);

  api.runtime().request_stop();
  EXPECT_TRUE(api.runtime().finish());
  EXPECT_EQ(api.runtime().lifecycle().state(), LifecycleState::stopped);
  EXPECT_EQ(worker.runtime().lifecycle().state(), LifecycleState::running);
}

TEST(RuntimeOwnerTest, DependenciesAreDestroyedInReverseConstructionOrder) {
  ApiRuntimeOwner api{config()};
  std::vector<int> destroyed;
  api.runtime().dependencies().emplace<TrackedDependency>(destroyed, 1);
  api.runtime().dependencies().emplace<TrackedDependency>(destroyed, 2);
  api.runtime().dependencies().emplace<TrackedDependency>(destroyed, 3);

  api.runtime().request_stop();
  ASSERT_TRUE(api.runtime().finish());

  EXPECT_EQ(destroyed, (std::vector<int>{3, 2, 1}));
}

TEST(RuntimeOwnerTest, ConstructionFailureIsSanitizedAndExistingOwnersRemainSafe) {
  WorkerRuntimeOwner worker{config()};
  std::vector<int> destroyed;
  worker.runtime().dependencies().emplace<TrackedDependency>(destroyed, 1);

  try {
    static_cast<void>(worker.runtime().dependencies().emplace_named<FailingDependency>(
        "postgres", "database-password"));
    FAIL() << "expected DependencyConstructionError";
  } catch (const DependencyConstructionError &error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("postgres"), std::string::npos);
    EXPECT_EQ(message.find("database-password"), std::string::npos);
  }

  EXPECT_EQ(worker.runtime().dependencies().size(), 1U);
  worker.runtime().request_stop();
  EXPECT_TRUE(worker.runtime().finish());
  EXPECT_EQ(destroyed, (std::vector<int>{1}));
}

} // namespace
