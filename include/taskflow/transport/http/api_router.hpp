#pragma once

#include "taskflow/application/authentication_middleware.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if TASKFLOW_HAS_DROGON
namespace drogon {
class HttpAppFramework;
}
#endif

namespace taskflow::transport::http {

class IdentityController;
class MembershipController;
class ProjectController;
class AuditController;
class CommentController;
class TaskController;

inline constexpr std::string_view api_prefix = "/api/v1";
inline constexpr std::size_t maximum_request_body_bytes = 1024U * 1024U;
inline constexpr std::string_view request_id_attribute = "taskflow.request_id";
inline constexpr std::string_view principal_attribute = "taskflow.authenticated_principal";

struct ErrorDetail {
  std::string field;
  std::string code;
  std::string message;
};

struct ApiError {
  int status;
  std::string code;
  std::string message;
  std::vector<ErrorDetail> details;
  std::string request_id;
};

struct RequestValidation {
  std::string request_id;
  std::optional<ApiError> error;
};

struct RouteAuthentication {
  std::optional<application::AuthenticatedPrincipal> principal;
  std::optional<ApiError> error;
};

struct ReadinessReport {
  bool ready;
  std::string postgres;
  std::string schema;
};

using ReadinessCheck = std::function<ReadinessReport()>;

[[nodiscard]] std::string serialize_error(const ApiError &error);
[[nodiscard]] bool is_json_content_type(std::string_view content_type) noexcept;
[[nodiscard]] RequestValidation validate_request(std::string_view method, std::size_t body_size,
                                                 std::string_view content_type,
                                                 std::string_view supplied_request_id);
[[nodiscard]] bool route_requires_authentication(std::string_view method,
                                                 std::string_view path) noexcept;
[[nodiscard]] RouteAuthentication
authenticate_route(std::string_view method, std::string_view path, std::string_view authorization,
                   std::string_view request_id,
                   const application::AuthenticationMiddleware *authentication) noexcept;
[[nodiscard]] std::string serialize_liveness();
[[nodiscard]] std::string serialize_readiness(const ReadinessReport &report);
[[nodiscard]] std::map<std::string, std::string> security_headers(bool external_tls);
[[nodiscard]] bool cors_origin_allowed(std::string_view origin,
                                       const std::vector<std::string> &allowed_origins) noexcept;

#if TASKFLOW_HAS_DROGON
void configure_api_router(drogon::HttpAppFramework &application, ReadinessCheck readiness_check,
                          const IdentityController *identity = nullptr,
                          const application::AuthenticationMiddleware *authentication = nullptr,
                          const ProjectController *projects = nullptr,
                          const MembershipController *memberships = nullptr,
                          const TaskController *tasks = nullptr,
                          const CommentController *comments = nullptr,
                          const AuditController *audit = nullptr,
                          const std::atomic_bool *accepting_requests = nullptr);
#endif

} // namespace taskflow::transport::http
