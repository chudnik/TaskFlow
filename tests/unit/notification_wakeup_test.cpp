#include "taskflow/application/notification_wakeup.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow;

class Wakeup final : public application::NotificationWakeup {
public:
  bool available{false};
  bool publish(const domain::Uuid &) noexcept override { return available; }
};

TEST(NotificationWakeupTest, FallsBackToPostgresAndRecovers) {
  Wakeup wakeup;
  application::WakeupCoordinator coordinator{wakeup};
  EXPECT_FALSE(coordinator.notify(domain::Uuid::generate()));
  EXPECT_TRUE(coordinator.postgres_poll_required());
  wakeup.available = true;
  EXPECT_TRUE(coordinator.notify(domain::Uuid::generate()));
  coordinator.redis_recovered();
  EXPECT_FALSE(coordinator.postgres_poll_required());
}

} // namespace
