#pragma once

#include <cstddef>
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

inline constexpr std::string_view api_prefix = "/api/v1";
inline constexpr std::size_t maximum_request_body_bytes = 1024U * 1024U;
inline constexpr std::string_view request_id_attribute = "taskflow.request_id";

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

[[nodiscard]] std::string serialize_error(const ApiError &error);
[[nodiscard]] bool is_json_content_type(std::string_view content_type) noexcept;
[[nodiscard]] RequestValidation validate_request(std::string_view method, std::size_t body_size,
                                                 std::string_view content_type,
                                                 std::string_view supplied_request_id);

#if TASKFLOW_HAS_DROGON
void configure_api_router(drogon::HttpAppFramework &application);
#endif

} // namespace taskflow::transport::http
