#include "taskflow/transport/http/api_router.hpp"

#include "taskflow/transport/http/audit_controller.hpp"
#include "taskflow/transport/http/comment_controller.hpp"
#include "taskflow/transport/http/identity_controller.hpp"
#include "taskflow/transport/http/membership_controller.hpp"
#include "taskflow/transport/http/project_controller.hpp"
#include "taskflow/transport/http/task_controller.hpp"

#if TASKFLOW_HAS_DROGON
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <charconv>
#include <memory>

namespace taskflow::transport::http {
namespace {

[[nodiscard]] drogon::HttpResponsePtr make_response(const ApiError &error) {
  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(static_cast<drogon::HttpStatusCode>(error.status));
  response->setContentTypeString("application/json; charset=utf-8");
  response->setBody(serialize_error(error));
  response->addHeader("X-Request-ID", error.request_id);
  return response;
}

[[nodiscard]] drogon::HttpResponsePtr json_response(const drogon::HttpStatusCode status,
                                                    std::string body,
                                                    const std::string &request_id) {
  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(status);
  response->setContentTypeString("application/json; charset=utf-8");
  response->setBody(std::move(body));
  response->addHeader("X-Request-ID", request_id);
  return response;
}

[[nodiscard]] drogon::HttpResponsePtr controller_response(const ControllerResponse &result,
                                                          const std::string &request_id) {
  auto response = drogon::HttpResponse::newHttpResponse();
  response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.status));
  if (!result.body.empty()) {
    response->setContentTypeString("application/json; charset=utf-8");
    response->setBody(result.body);
  }
  response->addHeader("X-Request-ID", request_id);
  return response;
}

[[nodiscard]] std::string request_id_from(const drogon::HttpRequestPtr &request) {
  const auto stored = request->getAttributes()->get<std::string>(std::string{request_id_attribute});
  return stored.empty()
             ? validate_request("GET", 0, "", request->getHeader("x-request-id")).request_id
             : stored;
}

[[nodiscard]] application::AuthenticatedPrincipal
principal_from(const drogon::HttpRequestPtr &request) {
  return *request->getAttributes()->get<std::shared_ptr<const application::AuthenticatedPrincipal>>(
      std::string{principal_attribute});
}

[[nodiscard]] std::optional<domain::PageRequest> page_from(const drogon::HttpRequestPtr &request) {
  std::optional<std::size_t> size;
  if (const auto value = request->getParameter("page_size"); !value.empty()) {
    std::size_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
      return std::nullopt;
    size = parsed;
  }
  const auto cursor = request->getParameter("cursor");
  return domain::PageRequest::create(size, cursor.empty() ? std::nullopt
                                                          : std::optional<std::string>{cursor});
}

[[nodiscard]] std::optional<application::NormalizedTaskQuery>
task_query_from(const drogon::HttpRequestPtr &request) {
  const auto project = domain::Uuid::parse(request->getParameter("project_id"));
  if (!project)
    return std::nullopt;
  application::TaskFilters filters{*project, {}, {}, {}, {}, {}, {}, {}, {}};
  if (const auto value = request->getParameter("status"); !value.empty()) {
    filters.status = domain::parse_task_status(value);
    if (!filters.status)
      return std::nullopt;
  }
  if (const auto value = request->getParameter("priority"); !value.empty()) {
    filters.priority = domain::parse_task_priority(value);
    if (!filters.priority)
      return std::nullopt;
  }
  const auto parse_uuid = [&](const char *name, std::optional<domain::Uuid> &target) {
    const auto value = request->getParameter(name);
    if (value.empty())
      return true;
    target = domain::Uuid::parse(value);
    return target.has_value();
  };
  if (!parse_uuid("assignee_id", filters.assignee_id) ||
      !parse_uuid("creator_id", filters.creator_id))
    return std::nullopt;
  const auto parse_instant = [&](const char *name, std::optional<domain::UtcInstant> &target) {
    const auto value = request->getParameter(name);
    if (value.empty())
      return true;
    target = domain::parse_utc(value);
    return target.has_value();
  };
  if (!parse_instant("deadline_from", filters.deadline_from) ||
      !parse_instant("deadline_to", filters.deadline_to))
    return std::nullopt;
  if (const auto value = request->getParameter("overdue"); !value.empty()) {
    if (value != "true" && value != "false")
      return std::nullopt;
    filters.overdue = value == "true";
  }
  if (const auto value = request->getParameter("title"); !value.empty())
    filters.title = value;
  application::TaskOrder order;
  if (const auto value = request->getParameter("sort"); !value.empty()) {
    const auto field = application::parse_task_sort_field(value);
    if (!field)
      return std::nullopt;
    order.field = *field;
  }
  return application::normalize_task_query(std::move(filters), order);
}

[[nodiscard]] ControllerResponse invalid_query() {
  return {
      400,
      R"({"error":{"code":"invalid_query","message":"query parameters are invalid","details":[]}})"};
}

} // namespace

void configure_api_router(drogon::HttpAppFramework &application, ReadinessCheck readiness_check,
                          const IdentityController *identity,
                          const application::AuthenticationMiddleware *authentication,
                          const ProjectController *projects,
                          const MembershipController *memberships, const TaskController *tasks,
                          const CommentController *comments, const AuditController *audit,
                          const std::atomic_bool *accepting_requests) {
  application.setClientMaxBodySize(maximum_request_body_bytes)
      .setClientMaxMemoryBodySize(maximum_request_body_bytes)
      .registerSyncAdvice([authentication,
                           accepting_requests](const drogon::HttpRequestPtr &request) {
        const auto validation = validate_request(request->methodString(), request->bodyLength(),
                                                 request->getHeader("content-type"),
                                                 request->getHeader("x-request-id"));
        request->getAttributes()->insert(std::string{request_id_attribute}, validation.request_id);
        if (accepting_requests != nullptr && !accepting_requests->load(std::memory_order_acquire) &&
            !request->path().starts_with("/health/")) {
          return make_response(ApiError{
              503, "shutting_down", "service is shutting down", {}, validation.request_id});
        }
        if (validation.error)
          return make_response(*validation.error);
        auto route_authentication = authenticate_route(request->methodString(), request->path(),
                                                       request->getHeader("authorization"),
                                                       validation.request_id, authentication);
        if (route_authentication.error)
          return make_response(*route_authentication.error);
        if (route_authentication.principal)
          request->getAttributes()->insert(
              std::string{principal_attribute},
              std::make_shared<const application::AuthenticatedPrincipal>(
                  *route_authentication.principal));
        return drogon::HttpResponsePtr{};
      })
      .registerPreSendingAdvice(
          [](const drogon::HttpRequestPtr &request, const drogon::HttpResponsePtr &response) {
            response->addHeader("X-Request-ID", request_id_from(request));
            for (const auto &[name, value] : security_headers(true))
              response->addHeader(name, value);
          })
      .setCustomErrorHandler(
          [](const drogon::HttpStatusCode status, const drogon::HttpRequestPtr &request) {
            const int code = static_cast<int>(status);
            return make_response(ApiError{code,
                                          code == 404 ? "not_found" : "http_error",
                                          code == 404 ? "resource not found" : "request failed",
                                          {},
                                          request_id_from(request)});
          });

  application.registerHandler(std::string{api_prefix},
                              [](const drogon::HttpRequestPtr &request,
                                 std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
                                auto response = drogon::HttpResponse::newHttpResponse();
                                response->setStatusCode(drogon::k200OK);
                                response->setContentTypeString("application/json; charset=utf-8");
                                response->setBody("{\"version\":\"v1\"}");
                                response->addHeader("X-Request-ID", request_id_from(request));
                                callback(response);
                              },
                              {drogon::Get});

  application.registerHandler(
      "/health/live",
      [](const drogon::HttpRequestPtr &request,
         std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        callback(json_response(drogon::k200OK, serialize_liveness(), request_id_from(request)));
      },
      {drogon::Get});

  application.registerHandler(
      "/health/ready",
      [check = std::move(readiness_check)](
          const drogon::HttpRequestPtr &request,
          std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        const auto report = check();
        callback(json_response(report.ready ? drogon::k200OK : drogon::k503ServiceUnavailable,
                               serialize_readiness(report), request_id_from(request)));
      },
      {drogon::Get});

  if (identity != nullptr) {
    const auto register_identity_route = [&application, identity](
                                             const std::string &path,
                                             ControllerResponse (IdentityController::*operation)(
                                                 std::string_view) const,
                                             const bool authorization_header) {
      application.registerHandler(
          path,
          [identity, operation,
           authorization_header](const drogon::HttpRequestPtr &request,
                                 std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            const auto input = authorization_header ? request->getHeader("authorization")
                                                    : std::string{request->body()};
            callback(controller_response((identity->*operation)(input), request_id_from(request)));
          },
          {drogon::Post});
    };
    register_identity_route("/api/v1/auth/register", &IdentityController::register_user, false);
    register_identity_route("/api/v1/auth/login", &IdentityController::login, false);
    register_identity_route("/api/v1/auth/refresh", &IdentityController::refresh, false);
    register_identity_route("/api/v1/auth/logout", &IdentityController::logout, true);
  }

  if (projects != nullptr) {
    application.registerHandler(
        "/api/v1/projects",
        [projects](const drogon::HttpRequestPtr &request,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
          callback(controller_response(projects->list(principal_from(request)),
                                       request_id_from(request)));
        },
        {drogon::Get});
    application.registerHandler(
        "/api/v1/projects",
        [projects](const drogon::HttpRequestPtr &request,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
          callback(controller_response(projects->create(principal_from(request), request->body()),
                                       request_id_from(request)));
        },
        {drogon::Post});
    application.registerHandler(
        "/api/v1/projects/{1}",
        [projects](const drogon::HttpRequestPtr &request,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                   const std::string &project_id) {
          callback(controller_response(projects->read(principal_from(request), project_id),
                                       request_id_from(request)));
        },
        {drogon::Get});
    application.registerHandler(
        "/api/v1/projects/{1}",
        [projects](const drogon::HttpRequestPtr &request,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                   const std::string &project_id) {
          callback(controller_response(
              projects->update(principal_from(request), project_id, request->body()),
              request_id_from(request)));
        },
        {drogon::Put});
    application.registerHandler(
        "/api/v1/projects/{1}",
        [projects](const drogon::HttpRequestPtr &request,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                   const std::string &project_id) {
          callback(controller_response(projects->archive(principal_from(request), project_id),
                                       request_id_from(request)));
        },
        {drogon::Delete});
  }

  if (memberships != nullptr) {
    application.registerHandler(
        "/api/v1/projects/{1}/members",
        [memberships](const drogon::HttpRequestPtr &request,
                      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                      const std::string &project_id) {
          callback(controller_response(memberships->list(principal_from(request), project_id),
                                       request_id_from(request)));
        },
        {drogon::Get});
    application.registerHandler(
        "/api/v1/projects/{1}/members",
        [memberships](const drogon::HttpRequestPtr &request,
                      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                      const std::string &project_id) {
          callback(controller_response(
              memberships->add(principal_from(request), project_id, request->body()),
              request_id_from(request)));
        },
        {drogon::Post});
    application.registerHandler(
        "/api/v1/projects/{1}/members/{2}",
        [memberships](const drogon::HttpRequestPtr &request,
                      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                      const std::string &project_id, const std::string &user_id) {
          callback(controller_response(memberships->change_role(principal_from(request), project_id,
                                                                user_id, request->body()),
                                       request_id_from(request)));
        },
        {drogon::Patch});
    application.registerHandler(
        "/api/v1/projects/{1}/members/{2}",
        [memberships](const drogon::HttpRequestPtr &request,
                      std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                      const std::string &project_id, const std::string &user_id) {
          callback(
              controller_response(memberships->remove(principal_from(request), project_id, user_id),
                                  request_id_from(request)));
        },
        {drogon::Delete});
  }

  if (tasks != nullptr) {
    application.registerHandler(
        "/api/v1/tasks",
        [tasks](const drogon::HttpRequestPtr &request,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
          const auto query = task_query_from(request);
          const auto page = page_from(request);
          callback(controller_response(
              query && page ? tasks->list(principal_from(request), *query, *page) : invalid_query(),
              request_id_from(request)));
        },
        {drogon::Get});
    application.registerHandler(
        "/api/v1/tasks",
        [tasks](const drogon::HttpRequestPtr &request,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
          callback(controller_response(tasks->create(principal_from(request), request->body()),
                                       request_id_from(request)));
        },
        {drogon::Post});
    const auto register_task_route = [&application,
                                      tasks](const std::string &path,
                                             ControllerResponse (TaskController::*operation)(
                                                 const application::AuthenticatedPrincipal &,
                                                 std::string_view, std::string_view) const,
                                             const drogon::HttpMethod method) {
      application.registerHandler(
          path,
          [tasks, operation](const drogon::HttpRequestPtr &request,
                             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                             const std::string &task_id) {
            callback(controller_response(
                (tasks->*operation)(principal_from(request), task_id, request->body()),
                request_id_from(request)));
          },
          {method});
    };
    application.registerHandler(
        "/api/v1/tasks/{1}",
        [tasks](const drogon::HttpRequestPtr &request,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                const std::string &task_id) {
          callback(controller_response(tasks->read(principal_from(request), task_id),
                                       request_id_from(request)));
        },
        {drogon::Get});
    register_task_route("/api/v1/tasks/{1}", &TaskController::update, drogon::Put);
    register_task_route("/api/v1/tasks/{1}", &TaskController::remove, drogon::Delete);
    register_task_route("/api/v1/tasks/{1}/status", &TaskController::transition, drogon::Patch);
    register_task_route("/api/v1/tasks/{1}/assignee", &TaskController::assign, drogon::Patch);
  }

  if (comments != nullptr) {
    application.registerHandler(
        "/api/v1/tasks/{1}/comments",
        [comments](const drogon::HttpRequestPtr &request,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                   const std::string &task_id) {
          callback(controller_response(comments->list(principal_from(request), task_id),
                                       request_id_from(request)));
        },
        {drogon::Get});
    application.registerHandler(
        "/api/v1/tasks/{1}/comments",
        [comments](const drogon::HttpRequestPtr &request,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                   const std::string &task_id) {
          callback(controller_response(
              comments->create(principal_from(request), task_id, request->body()),
              request_id_from(request)));
        },
        {drogon::Post});
    application.registerHandler(
        "/api/v1/comments/{1}",
        [comments](const drogon::HttpRequestPtr &request,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                   const std::string &comment_id) {
          callback(controller_response(
              comments->edit(principal_from(request), comment_id, request->body()),
              request_id_from(request)));
        },
        {drogon::Patch});
    application.registerHandler(
        "/api/v1/comments/{1}",
        [comments](const drogon::HttpRequestPtr &request,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                   const std::string &comment_id) {
          callback(controller_response(comments->remove(principal_from(request), comment_id),
                                       request_id_from(request)));
        },
        {drogon::Delete});
  }

  if (audit != nullptr) {
    application.registerHandler(
        "/api/v1/projects/{1}/history",
        [audit](const drogon::HttpRequestPtr &request,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                const std::string &project_id) {
          const auto page = page_from(request);
          callback(controller_response(
              page ? audit->history(principal_from(request), project_id, std::nullopt, *page)
                   : invalid_query(),
              request_id_from(request)));
        },
        {drogon::Get});
    application.registerHandler(
        "/api/v1/tasks/{1}/history",
        [audit](const drogon::HttpRequestPtr &request,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                const std::string &task_id) {
          const auto page = page_from(request);
          callback(controller_response(
              page ? audit->task_history(principal_from(request), task_id, *page) : invalid_query(),
              request_id_from(request)));
        },
        {drogon::Get});
  }
}

} // namespace taskflow::transport::http
#endif
