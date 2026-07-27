#include "taskflow/transport/http/audit_controller.hpp"

#include "taskflow/application/task_cursor.hpp"
#include "taskflow/application/tasks.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::transport::http {

AuditController::AuditController(const application::AuditUseCases &use_cases)
    : use_cases_{&use_cases} {}

ControllerResponse AuditController::history(const application::AuthenticatedPrincipal &actor,
                                            const std::string_view project_id,
                                            const std::optional<std::string_view> task_id,
                                            const domain::PageRequest page) const {
  const auto project = domain::Uuid::parse(project_id);
  const auto task = task_id ? domain::Uuid::parse(*task_id) : std::optional<domain::Uuid>{};
  if (!project || (task_id && !task))
    return {
        400,
        R"({"error":{"code":"invalid_id","message":"project/task ID must be UUID","details":[]}})"};
  try {
    const auto result = use_cases_->history(actor, *project, task, page);
    nlohmann::json body{{"items", nlohmann::json::array()}};
    for (const auto &event : result.items) {
      body["items"].push_back(
          {{"id", event.id},
           {"event_id", event.event_id.to_string()},
           {"project_id", event.project_id.to_string()},
           {"task_id",
            event.task_id ? nlohmann::json(event.task_id->to_string()) : nlohmann::json(nullptr)},
           {"actor_user_id", event.actor_user_id ? nlohmann::json(event.actor_user_id->to_string())
                                                 : nlohmann::json(nullptr)},
           {"event_type", event.event_type},
           {"entity_type", event.entity_type},
           {"entity_id", event.entity_id.to_string()},
           {"before", event.before},
           {"after", event.after},
           {"occurred_at", domain::format_utc(event.occurred_at)}});
    }
    body["next_cursor"] = result.next_before_id
                              ? nlohmann::json(std::to_string(*result.next_before_id))
                              : nlohmann::json(nullptr);
    return {200, body.dump()};
  } catch (const application::TaskError &error) {
    return {404, nlohmann::json{{"error",
                                 {{"code", "history_not_found"},
                                  {"message", error.what()},
                                  {"details", nlohmann::json::array()}}}}
                     .dump()};
  } catch (const application::CursorError &error) {
    return {400, nlohmann::json{{"error",
                                 {{"code", "invalid_cursor"},
                                  {"message", error.what()},
                                  {"details", nlohmann::json::array()}}}}
                     .dump()};
  }
}

} // namespace taskflow::transport::http
