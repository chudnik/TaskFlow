#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace taskflow::platform {

class StopController {
public:
  void request_stop() noexcept;
  [[nodiscard]] bool stop_requested() const noexcept;
  [[nodiscard]] bool wait_for(std::chrono::milliseconds duration);

private:
  std::atomic_bool stopping_{false};
  std::mutex mutex_;
  std::condition_variable condition_;
};

class BoundedBackoff {
public:
  BoundedBackoff(std::chrono::milliseconds initial, std::chrono::milliseconds maximum);

  [[nodiscard]] std::chrono::milliseconds next_delay() noexcept;
  void reset() noexcept;

private:
  std::chrono::milliseconds initial_;
  std::chrono::milliseconds maximum_;
  std::chrono::milliseconds next_;
};

enum class LifecycleState : std::uint8_t { starting, running, stopping, stopped };

class Lifecycle {
public:
  [[nodiscard]] LifecycleState state() const noexcept;
  [[nodiscard]] bool mark_running() noexcept;
  [[nodiscard]] bool request_stop() noexcept;
  [[nodiscard]] bool mark_stopped() noexcept;

private:
  std::atomic<LifecycleState> state_{LifecycleState::starting};
};

} // namespace taskflow::platform
