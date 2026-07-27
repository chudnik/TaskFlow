#include "taskflow/application/task_cursor.hpp"

#include <utility>

namespace taskflow::application {

CursorError::CursorError(const CursorErrorCode code, std::string message)
    : std::runtime_error{std::move(message)}, code_{code} {}
CursorErrorCode CursorError::code() const noexcept { return code_; }

} // namespace taskflow::application
