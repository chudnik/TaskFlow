#include "taskflow/transport/http/comment_controller.hpp"

#include <nlohmann/json.hpp>

namespace taskflow::transport::http {
namespace {

nlohmann::json json(const domain::Comment &comment) {
  return {{"id", comment.id.to_string()},
          {"task_id", comment.task_id.to_string()},
          {"author_id", comment.author_id.to_string()},
          {"body", comment.body},
          {"created_at", domain::format_utc(comment.created_at)},
          {"updated_at", domain::format_utc(comment.updated_at)}};
}

ControllerResponse error(const application::TaskError &value) {
  const bool missing = value.code() == application::TaskErrorCode::not_found;
  const bool forbidden = value.code() == application::TaskErrorCode::forbidden;
  return {missing     ? 404
          : forbidden ? 403
                      : 422,
          nlohmann::json{{"error",
                          {{"code", missing     ? "comment_not_found"
                                    : forbidden ? "comment_forbidden"
                                                : "invalid_comment"},
                           {"message", value.what()},
                           {"details", nlohmann::json::array()}}}}
              .dump()};
}

template <class Callback> ControllerResponse with_id(std::string_view value, Callback callback) {
  const auto id = domain::Uuid::parse(value);
  if (!id)
    return {400, R"({"error":{"code":"invalid_id","message":"ID must be a UUID","details":[]}})"};
  try {
    return callback(*id);
  } catch (const nlohmann::json::exception &) {
    return {
        400,
        R"({"error":{"code":"invalid_json","message":"body must be valid JSON","details":[]}})"};
  } catch (const application::TaskError &task_error) {
    return error(task_error);
  }
}

} // namespace

CommentController::CommentController(const application::CommentUseCases &use_cases)
    : use_cases_{&use_cases} {}

ControllerResponse CommentController::create(const application::AuthenticatedPrincipal &actor,
                                             const std::string_view task_id,
                                             const std::string_view json_body) const {
  return with_id(task_id, [&](const auto &id) {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.contains("body") || !body["body"].is_string())
      return ControllerResponse{
          400, R"({"error":{"code":"invalid_request","message":"body is required","details":[]}})"};
    return ControllerResponse{
        201, json(use_cases_->create(actor, id, body["body"].get<std::string>())).dump()};
  });
}

ControllerResponse CommentController::list(const application::AuthenticatedPrincipal &actor,
                                           const std::string_view task_id) const {
  return with_id(task_id, [&](const auto &id) {
    nlohmann::json body{{"items", nlohmann::json::array()}};
    for (const auto &comment : use_cases_->list(actor, id))
      body["items"].push_back(json(comment));
    return ControllerResponse{200, body.dump()};
  });
}

ControllerResponse CommentController::edit(const application::AuthenticatedPrincipal &actor,
                                           const std::string_view comment_id,
                                           const std::string_view json_body) const {
  return with_id(comment_id, [&](const auto &id) {
    const auto body = nlohmann::json::parse(json_body);
    if (!body.contains("body") || !body["body"].is_string())
      return ControllerResponse{
          400, R"({"error":{"code":"invalid_request","message":"body is required","details":[]}})"};
    return ControllerResponse{
        200, json(use_cases_->edit(actor, id, body["body"].get<std::string>())).dump()};
  });
}

ControllerResponse CommentController::remove(const application::AuthenticatedPrincipal &actor,
                                             const std::string_view comment_id) const {
  return with_id(comment_id, [&](const auto &id) {
    use_cases_->remove(actor, id);
    return ControllerResponse{204, {}};
  });
}

} // namespace taskflow::transport::http
