#include "taskflow/infrastructure/redis_notification_subscriber.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#if TASKFLOW_HAS_REDIS
#include <sw/redis++/redis++.h>
#endif

namespace taskflow::infrastructure {

struct RedisNotificationSubscriber::Impl {
  Impl(std::string value, std::function<void()> callback)
      : uri{std::move(value)}, wake{std::move(callback)}, thread{[this] { run(); }} {}

  ~Impl() {
    stopping = true;
    condition.notify_all();
#if TASKFLOW_HAS_REDIS
    try {
      sw::redis::Redis redis{uri};
      static_cast<void>(redis.publish("taskflow.notifications", "__shutdown__"));
    } catch (const sw::redis::Error &) {
    }
#endif
  }

  void run() {
#if TASKFLOW_HAS_REDIS
    while (!stopping) {
      try {
        sw::redis::Redis redis{uri};
        auto subscriber = redis.subscriber();
        subscriber.on_message([this](std::string, std::string message) {
          if (message != "__shutdown__" && !stopping)
            wake();
        });
        subscriber.subscribe("taskflow.notifications");
        while (!stopping)
          subscriber.consume();
      } catch (const sw::redis::Error &) {
        std::unique_lock lock{mutex};
        condition.wait_for(lock, std::chrono::seconds{1}, [this] { return stopping.load(); });
      }
    }
#endif
  }

  std::string uri;
  std::function<void()> wake;
  std::atomic_bool stopping{false};
  std::mutex mutex;
  std::condition_variable condition;
  std::jthread thread;
};

RedisNotificationSubscriber::RedisNotificationSubscriber(std::string redis_uri,
                                                         std::function<void()> wake)
    : impl_{std::make_unique<Impl>(std::move(redis_uri), std::move(wake))} {}

RedisNotificationSubscriber::~RedisNotificationSubscriber() = default;

} // namespace taskflow::infrastructure
