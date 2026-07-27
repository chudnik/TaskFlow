#include "taskflow/transport/http/membership_controller.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::transport::http {
namespace {

[[nodiscard]] nlohmann::json membership_json(const domain::ProjectMembership &membership) {
  return {{"project_id", membership.project_id.to_string()},
          {"user_id", membership.user_id.to_string()},
          {"role", domain::project_role_name(membership.role)},
          {"joined_at", domain::format_utc(membership.joined_at)},
          {"updated_at", domain::format_utc(membership.updated_at)}};
}

[[nodiscard]] ControllerResponse error_response(const application::ProjectError &error) {
  int status = 500;
  std::string code = "internal_error";
  switch (error.code()) {
  case application::ProjectErrorCode::invalid_input:
    status = 422;
    code = "invalid_membership";
    break;
  case application::ProjectErrorCode::not_found:
    status = 404;
    code = "membership_not_found";
    break;
  case application::ProjectErrorCode::forbidden:
    status = 403;
    code = "membership_forbidden";
    break;
  case application::ProjectErrorCode::archived:
    status = 409;
    code = "project_archived";
    break;
  case application::ProjectErrorCode::conflict:
    status = 409;
    code = "membership_conflict";
    break;
  }
  return {status, nlohmann::json{{"error",
                                  {{"code", std::move(code)},
                                   {"message", error.what()},
                                   {"details", nlohmann::json::array()}}}}
                      .dump()};
}

[[nodiscard]] ControllerResponse invalid_id() {
  return {
      400,
      R"({"error":{"code":"invalid_id","message":"project and user IDs must be UUIDs","details":[]}})"};
}

[[nodiscard]] std::optional<domain::ProjectRole> role_from(const nlohmann::json &body) {
  if (!body.is_object() || !body.contains("role") || !body["role"].is_string()) {
    return std::nullopt;
  }
  return domain::parse_project_role(body["role"].get<std::string>());
}

} // namespace

MembershipController::MembershipController(const application::MembershipUseCases &use_cases)
    : use_cases_{&use_cases} {}

ControllerResponse MembershipController::add(const application::AuthenticatedPrincipal &actor,
                                             const std::string_view project_id,
                                             const std::string_view json_body) const {
  const auto project = domain::Uuid::parse(project_id);
  try {
    const auto body = nlohmann::json::parse(json_body);
    const auto user = body.is_object() && body.contains("user_id") && body["user_id"].is_string()
                          ? domain::Uuid::parse(body["user_id"].get<std::string>())
                          : std::nullopt;
    const auto role = role_from(body);
    if (!project || !user) {
      return invalid_id();
    }
    if (!role) {
      return {
          422,
          R"({"error":{"code":"invalid_membership","message":"role must be owner, manager, or member","details":[]}})"};
    }
    return {201, membership_json(use_cases_->add(actor, *project, *user, *role)).dump()};
  } catch (const nlohmann::json::exception &) {
    return {
        400,
        R"({"error":{"code":"invalid_json","message":"request body must be valid JSON","details":[]}})"};
  } catch (const application::ProjectError &error) {
    return error_response(error);
  }
}

ControllerResponse
MembershipController::change_role(const application::AuthenticatedPrincipal &actor,
                                  const std::string_view project_id, const std::string_view user_id,
                                  const std::string_view json_body) const {
  const auto project = domain::Uuid::parse(project_id);
  const auto user = domain::Uuid::parse(user_id);
  if (!project || !user) {
    return invalid_id();
  }
  try {
    const auto role = role_from(nlohmann::json::parse(json_body));
    if (!role) {
      return {
          422,
          R"({"error":{"code":"invalid_membership","message":"role must be owner, manager, or member","details":[]}})"};
    }
    return {200, membership_json(use_cases_->change_role(actor, *project, *user, *role)).dump()};
  } catch (const nlohmann::json::exception &) {
    return {
        400,
        R"({"error":{"code":"invalid_json","message":"request body must be valid JSON","details":[]}})"};
  } catch (const application::ProjectError &error) {
    return error_response(error);
  }
}

ControllerResponse MembershipController::list(const application::AuthenticatedPrincipal &actor,
                                              const std::string_view project_id) const {
  const auto project = domain::Uuid::parse(project_id);
  if (!project) {
    return invalid_id();
  }
  try {
    nlohmann::json body{{"items", nlohmann::json::array()}};
    for (const auto &membership : use_cases_->list(actor, *project)) {
      body["items"].push_back(membership_json(membership));
    }
    return {200, body.dump()};
  } catch (const application::ProjectError &error) {
    return error_response(error);
  }
}

ControllerResponse MembershipController::remove(const application::AuthenticatedPrincipal &actor,
                                                const std::string_view project_id,
                                                const std::string_view user_id) const {
  const auto project = domain::Uuid::parse(project_id);
  const auto user = domain::Uuid::parse(user_id);
  if (!project || !user) {
    return invalid_id();
  }
  try {
    use_cases_->remove(actor, *project, *user);
    return {204, {}};
  } catch (const application::ProjectError &error) {
    return error_response(error);
  }
}

} // namespace taskflow::transport::http
