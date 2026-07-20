#include "taskflow/transport/http/identity_controller.hpp"

#include "taskflow/domain/common.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::transport::http {
namespace {

[[nodiscard]] std::string public_user_json(const domain::User &user) {
  return nlohmann::json{{"id", user.id.to_string()},
                        {"email", user.email},
                        {"global_role", user.global_role == domain::GlobalRole::admin ? "admin"
                                                                                     : "user"},
                        {"status", user.status == domain::AccountStatus::active ? "active"
                                                                                : "inactive"},
                        {"created_at", domain::format_utc(user.created_at)},
                        {"updated_at", domain::format_utc(user.updated_at)}}
      .dump();
}

[[nodiscard]] ControllerResponse error_response(const int status, std::string code,
                                                std::string message) {
  return {status, nlohmann::json{{"error", {{"code", std::move(code)},
                                            {"message", std::move(message)},
                                            {"details", nlohmann::json::array()}}}}
                      .dump()};
}

} // namespace

IdentityController::IdentityController(const application::IdentityUseCases &use_cases)
    : use_cases_{&use_cases} {}

ControllerResponse IdentityController::register_user(const std::string_view json_body) const {
  return invoke(json_body, true);
}

ControllerResponse IdentityController::login(const std::string_view json_body) const {
  return invoke(json_body, false);
}

ControllerResponse IdentityController::invoke(const std::string_view json_body,
                                              const bool registration) const {
  try {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.is_object() || !body.contains("email") || !body["email"].is_string() ||
        !body.contains("password") || !body["password"].is_string()) {
      return error_response(400, "invalid_request", "email and password are required");
    }
    const auto user = registration
                          ? use_cases_->register_user(body["email"].get<std::string>(),
                                                     body["password"].get<std::string>())
                          : use_cases_->login(body["email"].get<std::string>(),
                                              body["password"].get<std::string>());
    return {registration ? 201 : 200, public_user_json(user)};
  } catch (const nlohmann::json::exception &) {
    return error_response(400, "invalid_json", "request body must be valid JSON");
  } catch (const application::IdentityError &error) {
    switch (error.code()) {
    case application::IdentityErrorCode::duplicate_email:
      return error_response(409, "duplicate_email", error.what());
    case application::IdentityErrorCode::invalid_credentials:
      return error_response(401, "invalid_credentials", error.what());
    case application::IdentityErrorCode::inactive_account:
      return error_response(403, "inactive_account", error.what());
    case application::IdentityErrorCode::invalid_input:
      return error_response(422, "invalid_input", error.what());
    }
  }
  return error_response(500, "internal_error", "request failed");
}

} // namespace taskflow::transport::http
