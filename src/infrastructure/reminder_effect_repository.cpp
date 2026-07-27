#include "taskflow/infrastructure/reminder_effect_repository.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::infrastructure {

ReminderEffectRepository::ReminderEffectRepository(PostgresConnection &connection)
    : connection_{&connection} {}

bool ReminderEffectRepository::emit_once(std::string effect_key, const domain::Uuid &recipient_id,
                                         const domain::Task &task,
                                         const application::ReminderKind kind) {
  const auto event_id = domain::Uuid::generate();
  auto transaction = connection_->transaction();
  const auto inserted =
      transaction.execute("INSERT INTO reminder_effects(effect_key,event_id) VALUES($1,$2::uuid) "
                          "ON CONFLICT(effect_key) DO NOTHING RETURNING effect_key",
                          {effect_key, event_id.to_string()});
  if (inserted.row_count() == 0) {
    transaction.rollback();
    return false;
  }
  const auto event_type =
      kind == application::ReminderKind::pre_deadline ? "task.pre_deadline" : "task.overdue";
  const auto payload = nlohmann::json{
      {"task_id", task.id.to_string()},
      {"recipient_id", recipient_id.to_string()},
      {"deadline_at", domain::format_utc(*task.deadline_at)},
      {"version",
       task.version}}.dump();
  static_cast<void>(transaction.execute(
      "INSERT INTO outbox_events(event_id,project_id,aggregate_type,aggregate_id,event_type,"
      "payload,correlation_id) VALUES($1::uuid,$2::uuid,'task',$3::uuid,$4,$5::jsonb,$6)",
      {event_id.to_string(), task.project_id.to_string(), task.id.to_string(), event_type, payload,
       std::move(effect_key)}));
  transaction.commit();
  return true;
}

} // namespace taskflow::infrastructure
