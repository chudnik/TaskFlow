#include "taskflow/transport/http/api_router.hpp"

#include "taskflow/domain/common.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace taskflow::transport::http {
namespace {

[[nodiscard]] std::string escape_json(const std::string_view value) {
  std::ostringstream output;
  for (const char raw_character : value) {
    const auto character = static_cast<unsigned char>(raw_character);
    switch (character) {
    case '"':
      output << "\\\"";
      break;
    case '\\':
      output << "\\\\";
      break;
    case '\b':
      output << "\\b";
      break;
    case '\f':
      output << "\\f";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      if (character < 0x20U) {
        output << "\\u00" << "0123456789abcdef"[character >> 4U]
               << "0123456789abcdef"[character & 0x0FU];
      } else {
        output << static_cast<char>(character);
      }
    }
  }
  return output.str();
}

[[nodiscard]] std::string normalized_request_id(const std::string_view supplied) {
  const bool valid = !supplied.empty() && supplied.size() <= 128 &&
                     std::all_of(supplied.begin(), supplied.end(), [](const char character) {
                       const auto value = static_cast<unsigned char>(character);
                       return std::isalnum(value) != 0 || character == '-' || character == '_' ||
                              character == '.' || character == ':';
                     });
  return valid ? std::string{supplied} : domain::Uuid::generate().to_string();
}

} // namespace

std::string serialize_error(const ApiError &error) {
  std::ostringstream output;
  output << "{\"error\":{\"code\":\"" << escape_json(error.code) << "\",\"message\":\""
         << escape_json(error.message) << "\",\"details\":[";
  for (std::size_t index = 0; index < error.details.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto &detail = error.details[index];
    output << "{\"field\":\"" << escape_json(detail.field) << "\",\"code\":\""
           << escape_json(detail.code) << "\",\"message\":\"" << escape_json(detail.message)
           << "\"}";
  }
  output << "],\"request_id\":\"" << escape_json(error.request_id) << "\"}}";
  return output.str();
}

bool is_json_content_type(const std::string_view content_type) noexcept {
  const auto separator = content_type.find(';');
  auto media_type = content_type.substr(0, separator);
  while (!media_type.empty() && std::isspace(static_cast<unsigned char>(media_type.front())) != 0) {
    media_type.remove_prefix(1);
  }
  while (!media_type.empty() && std::isspace(static_cast<unsigned char>(media_type.back())) != 0) {
    media_type.remove_suffix(1);
  }
  std::string normalized{media_type};
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](const char value) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
  });
  return normalized == "application/json";
}

RequestValidation validate_request(const std::string_view method, const std::size_t body_size,
                                   const std::string_view content_type,
                                   const std::string_view supplied_request_id) {
  RequestValidation validation{normalized_request_id(supplied_request_id), std::nullopt};
  if (body_size > maximum_request_body_bytes) {
    validation.error = ApiError{413, "request_too_large", "request body exceeds the limit", {},
                                validation.request_id};
    return validation;
  }
  const bool method_expects_json = method == "POST" || method == "PUT" || method == "PATCH";
  if (method_expects_json && body_size != 0 && !is_json_content_type(content_type)) {
    validation.error = ApiError{415, "unsupported_media_type",
                                "Content-Type must be application/json", {},
                                validation.request_id};
  }
  return validation;
}

} // namespace taskflow::transport::http
