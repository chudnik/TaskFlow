#pragma once

#include "taskflow/domain/common.hpp"

#include <optional>
#include <stdexcept>
#include <string>

namespace taskflow::application {

struct TaskCursor {
  int version{1};
  std::string query_fingerprint;
  std::optional<std::string> sort_value;
  domain::Uuid task_id;
};

enum class CursorErrorCode { malformed, invalid_signature, query_mismatch, unsupported_version };

class CursorError : public std::runtime_error {
public:
  CursorError(CursorErrorCode code, std::string message);
  [[nodiscard]] CursorErrorCode code() const noexcept;

private:
  CursorErrorCode code_;
};

class TaskCursorCodec {
public:
  virtual ~TaskCursorCodec() = default;
  [[nodiscard]] virtual std::string encode(const TaskCursor &cursor) const = 0;
  [[nodiscard]] virtual TaskCursor decode(std::string_view encoded,
                                          std::string_view expected_fingerprint) const = 0;
};

} // namespace taskflow::application
