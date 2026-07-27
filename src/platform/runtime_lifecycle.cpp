#include "taskflow/platform/runtime_lifecycle.hpp"

#include <algorithm>
#include <stdexcept>

namespace taskflow::platform {

void StopController::request_stop() noexcept {
  stopping_.store(true, std::memory_order_release);
  condition_.notify_all();
}

bool StopController::stop_requested() const noexcept {
  return stopping_.load(std::memory_order_acquire);
}

bool StopController::wait_for(const std::chrono::milliseconds duration) {
  std::unique_lock lock{mutex_};
  return condition_.wait_for(lock, duration, [this] { return stop_requested(); });
}

BoundedBackoff::BoundedBackoff(const std::chrono::milliseconds initial,
                               const std::chrono::milliseconds maximum)
    : initial_(initial), maximum_(maximum), next_(initial) {
  if (initial.count() <= 0 || maximum < initial) {
    throw std::invalid_argument{"backoff bounds must be positive and ordered"};
  }
}

std::chrono::milliseconds BoundedBackoff::next_delay() noexcept {
  const auto result = next_;
  const auto doubled = next_.count() > maximum_.count() / 2 ? maximum_ : next_ * 2;
  next_ = std::min(doubled, maximum_);
  return result;
}

void BoundedBackoff::reset() noexcept { next_ = initial_; }

LifecycleState Lifecycle::state() const noexcept { return state_.load(std::memory_order_acquire); }

bool Lifecycle::mark_running() noexcept {
  auto expected = LifecycleState::starting;
  return state_.compare_exchange_strong(expected, LifecycleState::running,
                                        std::memory_order_acq_rel);
}

bool Lifecycle::request_stop() noexcept {
  auto current = state();
  while (current != LifecycleState::stopping && current != LifecycleState::stopped) {
    if (state_.compare_exchange_weak(current, LifecycleState::stopping,
                                     std::memory_order_acq_rel)) {
      return true;
    }
  }
  return false;
}

bool Lifecycle::mark_stopped() noexcept {
  auto expected = LifecycleState::stopping;
  return state_.compare_exchange_strong(expected, LifecycleState::stopped,
                                        std::memory_order_acq_rel);
}

} // namespace taskflow::platform
