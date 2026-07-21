#include "taskflow/transport/http/project_controller.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::transport::http {
namespace {

[[nodiscard]] nlohmann::json project_json(const domain::Project &project) {
  nlohmann::json result{{"id", project.id.to_string()},
                        {"name", project.name},
                        {"description", project.description},
                        {"created_by", project.created_by.to_string()},
                        {"created_at", domain::format_utc(project.created_at)},
                        {"updated_at", domain::format_utc(project.updated_at)}};
  result["archived_at"] = project.archived_at
                              ? nlohmann::json{domain::format_utc(*project.archived_at)}
                              : nlohmann::json{nullptr};
  result["archived_by"] = project.archived_by ? nlohmann::json{project.archived_by->to_string()}
                                              : nlohmann::json{nullptr};
  return result;
}

[[nodiscard]] ControllerResponse error_response(const application::ProjectError &error) {
  int status = 500;
  std::string code = "internal_error";
  switch (error.code()) {
  case application::ProjectErrorCode::invalid_input:
    status = 422;
    code = "invalid_project";
    break;
  case application::ProjectErrorCode::not_found:
    status = 404;
    code = "project_not_found";
    break;
  case application::ProjectErrorCode::forbidden:
    status = 403;
    code = "project_forbidden";
    break;
  case application::ProjectErrorCode::archived:
    status = 409;
    code = "project_archived";
    break;
  }
  return {status, nlohmann::json{{"error",
                                  {{"code", std::move(code)},
                                   {"message", error.what()},
                                   {"details", nlohmann::json::array()}}}}
                      .dump()};
}

[[nodiscard]] std::optional<domain::Uuid> project_id_from(const std::string_view value) {
  return domain::Uuid::parse(value);
}

[[nodiscard]] ControllerResponse invalid_id() {
  return {400, nlohmann::json{{"error",
                               {{"code", "invalid_project_id"},
                                {"message", "project ID must be a UUID"},
                                {"details", nlohmann::json::array()}}}}
                   .dump()};
}

} // namespace

ProjectController::ProjectController(const application::ProjectUseCases &use_cases)
    : use_cases_{&use_cases} {}

ControllerResponse ProjectController::create(const application::AuthenticatedPrincipal &actor,
                                             const std::string_view json_body) const {
  try {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.is_object() || !body.contains("name") || !body["name"].is_string() ||
        (body.contains("description") && !body["description"].is_string())) {
      return {400,
              R"({"error":{"code":"invalid_request","message":"name is required","details":[]}})"};
    }
    const auto project = use_cases_->create(actor, body["name"].get<std::string>(),
                                            body.value("description", std::string{}));
    return {201, project_json(project).dump()};
  } catch (const nlohmann::json::exception &) {
    return {
        400,
        R"({"error":{"code":"invalid_json","message":"request body must be valid JSON","details":[]}})"};
  } catch (const application::ProjectError &error) {
    return error_response(error);
  }
}

ControllerResponse ProjectController::read(const application::AuthenticatedPrincipal &actor,
                                           const std::string_view project_id) const {
  const auto id = project_id_from(project_id);
  if (!id) {
    return invalid_id();
  }
  try {
    return {200, project_json(use_cases_->read(actor, *id)).dump()};
  } catch (const application::ProjectError &error) {
    return error_response(error);
  }
}

ControllerResponse ProjectController::update(const application::AuthenticatedPrincipal &actor,
                                             const std::string_view project_id,
                                             const std::string_view json_body) const {
  const auto id = project_id_from(project_id);
  if (!id) {
    return invalid_id();
  }
  try {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.is_object() || !body.contains("name") || !body["name"].is_string() ||
        !body.contains("description") || !body["description"].is_string()) {
      return {
          400,
          R"({"error":{"code":"invalid_request","message":"name and description are required","details":[]}})"};
    }
    return {200, project_json(use_cases_->update(actor, *id, body["name"].get<std::string>(),
                                                 body["description"].get<std::string>()))
                     .dump()};
  } catch (const nlohmann::json::exception &) {
    return {
        400,
        R"({"error":{"code":"invalid_json","message":"request body must be valid JSON","details":[]}})"};
  } catch (const application::ProjectError &error) {
    return error_response(error);
  }
}

ControllerResponse ProjectController::archive(const application::AuthenticatedPrincipal &actor,
                                              const std::string_view project_id) const {
  const auto id = project_id_from(project_id);
  if (!id) {
    return invalid_id();
  }
  try {
    return {200, project_json(use_cases_->archive(actor, *id)).dump()};
  } catch (const application::ProjectError &error) {
    return error_response(error);
  }
}

ControllerResponse ProjectController::list(const application::AuthenticatedPrincipal &actor) const {
  const auto projects = use_cases_->list(actor);
  nlohmann::json body{{"items", nlohmann::json::array()}};
  for (const auto &project : projects) {
    body["items"].push_back(project_json(project));
  }
  return {200, body.dump()};
}

} // namespace taskflow::transport::http
