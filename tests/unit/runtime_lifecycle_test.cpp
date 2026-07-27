#include "taskflow/platform/runtime_lifecycle.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace {

using namespace std::chrono_literals;
using taskflow::platform::BoundedBackoff;
using taskflow::platform::Lifecycle;
using taskflow::platform::LifecycleState;
using taskflow::platform::StopController;

TEST(RuntimeLifecycleTest, StopRequestInterruptsWaitAndIsIdempotent) {
  StopController stop;
  EXPECT_FALSE(stop.stop_requested());

  stop.request_stop();
  stop.request_stop();

  EXPECT_TRUE(stop.stop_requested());
  EXPECT_TRUE(stop.wait_for(1h));
}

TEST(RuntimeLifecycleTest, BackoffDoublesToBoundAndResets) {
  BoundedBackoff backoff{25ms, 100ms};

  EXPECT_EQ(backoff.next_delay(), 25ms);
  EXPECT_EQ(backoff.next_delay(), 50ms);
  EXPECT_EQ(backoff.next_delay(), 100ms);
  EXPECT_EQ(backoff.next_delay(), 100ms);

  backoff.reset();
  EXPECT_EQ(backoff.next_delay(), 25ms);
}

TEST(RuntimeLifecycleTest, BackoffRejectsInvalidBounds) {
  EXPECT_THROW(static_cast<void>(BoundedBackoff{0ms, 1ms}), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(BoundedBackoff{10ms, 5ms}), std::invalid_argument);
}

TEST(RuntimeLifecycleTest, LifecycleAllowsOnlyOrderedTransitions) {
  Lifecycle lifecycle;
  EXPECT_EQ(lifecycle.state(), LifecycleState::starting);
  EXPECT_FALSE(lifecycle.mark_stopped());
  EXPECT_TRUE(lifecycle.mark_running());
  EXPECT_FALSE(lifecycle.mark_running());
  EXPECT_TRUE(lifecycle.request_stop());
  EXPECT_FALSE(lifecycle.request_stop());
  EXPECT_TRUE(lifecycle.mark_stopped());
  EXPECT_EQ(lifecycle.state(), LifecycleState::stopped);
}

TEST(RuntimeLifecycleTest, StartupCanBeStoppedBeforeRunning) {
  Lifecycle lifecycle;
  EXPECT_TRUE(lifecycle.request_stop());
  EXPECT_FALSE(lifecycle.mark_running());
  EXPECT_TRUE(lifecycle.mark_stopped());
}

} // namespace
