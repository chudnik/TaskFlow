#pragma once

#include "taskflow/application/reminder_handlers.hpp"
#include "taskflow/infrastructure/postgres.hpp"

namespace taskflow::infrastructure {

class ReminderEffectRepository final : public application::ReminderEffectStore {
public:
  explicit ReminderEffectRepository(PostgresConnection &connection);
  [[nodiscard]] bool emit_once(std::string effect_key, const domain::Uuid &recipient_id,
                               const domain::Task &task, application::ReminderKind kind) override;

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
