#pragma once

#include <functional>
#include <memory>
#include <string>

namespace taskflow::infrastructure {

class RedisNotificationSubscriber {
public:
  RedisNotificationSubscriber(std::string redis_uri, std::function<void()> wake);
  ~RedisNotificationSubscriber();
  RedisNotificationSubscriber(const RedisNotificationSubscriber &) = delete;
  RedisNotificationSubscriber &operator=(const RedisNotificationSubscriber &) = delete;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace taskflow::infrastructure
