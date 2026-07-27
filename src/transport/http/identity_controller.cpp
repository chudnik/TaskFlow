#include "taskflow/transport/http/identity_controller.hpp"

#include "taskflow/domain/common.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::transport::http {
namespace {

[[nodiscard]] std::string public_user_json(const domain::User &user) {
  return nlohmann::json{
      {"id", user.id.to_string()},
      {"email", user.email},
      {"global_role", user.global_role == domain::GlobalRole::admin ? "admin" : "user"},
      {"status", user.status == domain::AccountStatus::active ? "active" : "inactive"},
      {"created_at", domain::format_utc(user.created_at)},
      {"updated_at", domain::format_utc(user.updated_at)}}
      .dump();
}

[[nodiscard]] std::string token_json(const application::AuthenticationTokens &tokens) {
  return nlohmann::json{{"access_token", tokens.access_token},
                        {"refresh_token", tokens.refresh_token},
                        {"token_type", "Bearer"},
                        {"access_expires_at", domain::format_utc(tokens.access_expires_at)},
                        {"refresh_expires_at", domain::format_utc(tokens.refresh_expires_at)},
                        {"user", nlohmann::json::parse(public_user_json(tokens.user))}}
      .dump();
}

[[nodiscard]] ControllerResponse error_response(const int status, std::string code,
                                                std::string message) {
  return {status, nlohmann::json{{"error",
                                  {{"code", std::move(code)},
                                   {"message", std::move(message)},
                                   {"details", nlohmann::json::array()}}}}
                      .dump()};
}

} // namespace

IdentityController::IdentityController(const application::IdentityUseCases &use_cases)
    : use_cases_{&use_cases} {}

IdentityController::IdentityController(const application::AuthenticationSessionUseCases &sessions,
                                       const application::AuthenticationMiddleware &authentication)
    : use_cases_{nullptr}, sessions_{&sessions}, authentication_{&authentication} {}

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
    const auto email = body["email"].get<std::string>();
    const auto password = body["password"].get<std::string>();
    if (sessions_ != nullptr) {
      const auto tokens = registration ? sessions_->register_user(email, password)
                                       : sessions_->login(email, password);
      return {registration ? 201 : 200, token_json(tokens)};
    }
    const auto user = registration ? use_cases_->register_user(email, password)
                                   : use_cases_->login(email, password);
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
  } catch (const std::exception &) {
    return error_response(500, "internal_error", "request failed");
  }
  return error_response(500, "internal_error", "request failed");
}

ControllerResponse IdentityController::refresh(const std::string_view json_body) const {
  if (sessions_ == nullptr) {
    return error_response(500, "internal_error", "request failed");
  }
  try {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.is_object() || !body.contains("refresh_token") ||
        !body["refresh_token"].is_string()) {
      return error_response(400, "invalid_request", "refresh_token is required");
    }
    return {200, token_json(sessions_->refresh(body["refresh_token"].get<std::string>()))};
  } catch (const nlohmann::json::exception &) {
    return error_response(400, "invalid_json", "request body must be valid JSON");
  } catch (const application::AuthenticationSessionError &) {
    return error_response(401, "invalid_refresh_token", "refresh token is invalid");
  } catch (const std::exception &) {
    return error_response(500, "internal_error", "request failed");
  }
}

ControllerResponse IdentityController::logout(const std::string_view authorization) const {
  if (sessions_ == nullptr || authentication_ == nullptr) {
    return error_response(500, "internal_error", "request failed");
  }
  const auto principal = authentication_->authenticate_bearer(authorization);
  if (!principal) {
    return error_response(401, "unauthorized", "valid access token is required");
  }
  try {
    sessions_->logout(principal->session_id);
    return {204, {}};
  } catch (const std::exception &) {
    return error_response(500, "internal_error", "request failed");
  }
}

} // namespace taskflow::transport::http
