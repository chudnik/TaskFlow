#include "taskflow/transport/http/task_controller.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::transport::http {
namespace {

[[nodiscard]] nlohmann::json task_json(const domain::Task &task,
                                       const domain::UtcInstant now = domain::SystemClock{}.now()) {
  nlohmann::json result{{"id", task.id.to_string()},
                        {"project_id", task.project_id.to_string()},
                        {"title", task.title},
                        {"description", task.description},
                        {"status", domain::task_status_name(task.status)},
                        {"priority", domain::task_priority_name(task.priority)},
                        {"creator_id", task.creator_id.to_string()},
                        {"version", task.version},
                        {"created_at", domain::format_utc(task.created_at)},
                        {"updated_at", domain::format_utc(task.updated_at)},
                        {"overdue", task.overdue(now)}};
  result["assignee_id"] =
      task.assignee_id ? nlohmann::json{task.assignee_id->to_string()} : nlohmann::json{nullptr};
  result["deadline_at"] = task.deadline_at ? nlohmann::json{domain::format_utc(*task.deadline_at)}
                                           : nlohmann::json{nullptr};
  result["completed_at"] = task.completed_at
                               ? nlohmann::json{domain::format_utc(*task.completed_at)}
                               : nlohmann::json{nullptr};
  return result;
}

[[nodiscard]] ControllerResponse task_error(const application::TaskError &error) {
  int status = 500;
  std::string code = "internal_error";
  switch (error.code()) {
  case application::TaskErrorCode::invalid_input:
    status = 422;
    code = "invalid_task";
    break;
  case application::TaskErrorCode::not_found:
    status = 404;
    code = "task_not_found";
    break;
  case application::TaskErrorCode::forbidden:
    status = 403;
    code = "task_forbidden";
    break;
  case application::TaskErrorCode::archived:
    status = 409;
    code = "project_archived";
    break;
  case application::TaskErrorCode::conflict:
    status = 409;
    code = "stale_task_version";
    break;
  case application::TaskErrorCode::invalid_transition:
    status = 422;
    code = "invalid_task_transition";
    break;
  case application::TaskErrorCode::invalid_assignee:
    status = 422;
    code = "invalid_task_assignee";
    break;
  }
  nlohmann::json details = nlohmann::json::array();
  if (error.current()) {
    details.push_back({{"current", task_json(*error.current())}});
  }
  return {status, nlohmann::json{{"error",
                                  {{"code", std::move(code)},
                                   {"message", error.what()},
                                   {"details", std::move(details)}}}}
                      .dump()};
}

[[nodiscard]] ControllerResponse invalid_request(const std::string &message) {
  return {400, nlohmann::json{{"error",
                               {{"code", "invalid_request"},
                                {"message", message},
                                {"details", nlohmann::json::array()}}}}
                   .dump()};
}

[[nodiscard]] std::optional<domain::Uuid> optional_uuid(const nlohmann::json &body,
                                                        const char *field) {
  if (!body.contains(field) || body[field].is_null()) {
    return std::nullopt;
  }
  return body[field].is_string() ? domain::Uuid::parse(body[field].get<std::string>())
                                 : std::nullopt;
}

[[nodiscard]] std::optional<domain::UtcInstant> optional_instant(const nlohmann::json &body,
                                                                 const char *field) {
  if (!body.contains(field) || body[field].is_null()) {
    return std::nullopt;
  }
  return body[field].is_string() ? domain::parse_utc(body[field].get<std::string>()) : std::nullopt;
}

template <typename Callback>
ControllerResponse with_task_id(const std::string_view id, Callback callback) {
  const auto parsed = domain::Uuid::parse(id);
  if (!parsed) {
    return invalid_request("task ID must be a UUID");
  }
  try {
    return callback(*parsed);
  } catch (const nlohmann::json::exception &) {
    return invalid_request("request body must be valid JSON");
  } catch (const application::TaskError &error) {
    return task_error(error);
  }
}

} // namespace

TaskController::TaskController(const application::TaskUseCases &use_cases)
    : use_cases_{&use_cases} {}

TaskController::TaskController(const application::TaskUseCases &use_cases,
                               const application::TaskListUseCase &list_use_case)
    : use_cases_{&use_cases}, list_use_case_{&list_use_case} {}

ControllerResponse TaskController::create(const application::AuthenticatedPrincipal &actor,
                                          const std::string_view json_body) const {
  try {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.is_object() || !body.contains("project_id") || !body["project_id"].is_string() ||
        !body.contains("title") || !body["title"].is_string()) {
      return invalid_request("project_id and title are required");
    }
    const auto project = domain::Uuid::parse(body["project_id"].get<std::string>());
    const auto priority = domain::parse_task_priority(body.value("priority", "medium"));
    if (!project || !priority) {
      return invalid_request("project_id or priority is invalid");
    }
    if (body.contains("assignee_id") && !body["assignee_id"].is_null() &&
        !optional_uuid(body, "assignee_id")) {
      return invalid_request("assignee_id must be a UUID or null");
    }
    if (body.contains("deadline_at") && !body["deadline_at"].is_null() &&
        !optional_instant(body, "deadline_at")) {
      return invalid_request("deadline_at must be an RFC 3339 UTC timestamp or null");
    }
    return {201, task_json(use_cases_->create(actor, {*project, body["title"].get<std::string>(),
                                                      body.value("description", std::string{}),
                                                      *priority, optional_uuid(body, "assignee_id"),
                                                      optional_instant(body, "deadline_at")}))
                     .dump()};
  } catch (const nlohmann::json::exception &) {
    return invalid_request("request body must be valid JSON");
  } catch (const application::TaskError &error) {
    return task_error(error);
  }
}

ControllerResponse TaskController::read(const application::AuthenticatedPrincipal &actor,
                                        const std::string_view task_id) const {
  return with_task_id(task_id, [&](const auto &id) {
    return ControllerResponse{200, task_json(use_cases_->read(actor, id)).dump()};
  });
}

ControllerResponse TaskController::update(const application::AuthenticatedPrincipal &actor,
                                          const std::string_view task_id,
                                          const std::string_view json_body) const {
  return with_task_id(task_id, [&](const auto &id) {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.contains("title") || !body["title"].is_string() || !body.contains("description") ||
        !body["description"].is_string() || !body.contains("priority") ||
        !body["priority"].is_string() || !body.contains("version") ||
        !body["version"].is_number_unsigned()) {
      return invalid_request("title, description, priority, and version are required");
    }
    const auto priority = domain::parse_task_priority(body["priority"].get<std::string>());
    if (!priority) {
      return invalid_request("priority is invalid");
    }
    return ControllerResponse{
        200, task_json(use_cases_->update(actor, id,
                                          {body["title"].get<std::string>(),
                                           body["description"].get<std::string>(), *priority,
                                           optional_instant(body, "deadline_at"),
                                           body["version"].get<std::uint64_t>()}))
                 .dump()};
  });
}

ControllerResponse TaskController::transition(const application::AuthenticatedPrincipal &actor,
                                              const std::string_view task_id,
                                              const std::string_view json_body) const {
  return with_task_id(task_id, [&](const auto &id) {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.contains("status") || !body["status"].is_string() || !body.contains("version") ||
        !body["version"].is_number_unsigned()) {
      return invalid_request("status and version are required");
    }
    const auto status = domain::parse_task_status(body["status"].get<std::string>());
    if (!status) {
      return invalid_request("status is invalid");
    }
    return ControllerResponse{
        200,
        task_json(use_cases_->transition(actor, id, *status, body["version"].get<std::uint64_t>()))
            .dump()};
  });
}

ControllerResponse TaskController::assign(const application::AuthenticatedPrincipal &actor,
                                          const std::string_view task_id,
                                          const std::string_view json_body) const {
  return with_task_id(task_id, [&](const auto &id) {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.contains("version") || !body["version"].is_number_unsigned()) {
      return invalid_request("version is required");
    }
    if (body.contains("assignee_id") && !body["assignee_id"].is_null() &&
        !optional_uuid(body, "assignee_id")) {
      return invalid_request("assignee_id must be a UUID or null");
    }
    return ControllerResponse{
        200, task_json(use_cases_->assign(actor, id, optional_uuid(body, "assignee_id"),
                                          body["version"].get<std::uint64_t>()))
                 .dump()};
  });
}

ControllerResponse TaskController::remove(const application::AuthenticatedPrincipal &actor,
                                          const std::string_view task_id,
                                          const std::string_view json_body) const {
  return with_task_id(task_id, [&](const auto &id) {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.contains("version") || !body["version"].is_number_unsigned()) {
      return invalid_request("version is required");
    }
    use_cases_->remove(actor, id, body["version"].get<std::uint64_t>());
    return ControllerResponse{204, {}};
  });
}

ControllerResponse TaskController::list(const application::AuthenticatedPrincipal &actor,
                                        application::NormalizedTaskQuery query,
                                        const domain::PageRequest page) const {
  if (list_use_case_ == nullptr) {
    return {
        500,
        R"({"error":{"code":"internal_error","message":"task list is unavailable","details":[]}})"};
  }
  try {
    const auto result = list_use_case_->list(actor, std::move(query), page);
    nlohmann::json body{{"items", nlohmann::json::array()}};
    for (const auto &task : result.items)
      body["items"].push_back(task_json(task));
    body["next_cursor"] =
        result.next_cursor ? nlohmann::json(*result.next_cursor) : nlohmann::json(nullptr);
    return {200, body.dump()};
  } catch (const application::CursorError &error) {
    return {400, nlohmann::json{{"error",
                                 {{"code", "invalid_cursor"},
                                  {"message", error.what()},
                                  {"details", nlohmann::json::array()}}}}
                     .dump()};
  } catch (const application::TaskError &error) {
    return task_error(error);
  }
}

} // namespace taskflow::transport::http
