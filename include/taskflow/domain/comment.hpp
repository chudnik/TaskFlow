#pragma once

#include "taskflow/domain/common.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace taskflow::domain {

struct Comment {
  Uuid id;
  Uuid task_id;
  Uuid author_id;
  std::string body;
  UtcInstant created_at;
  UtcInstant updated_at;
  std::optional<UtcInstant> deleted_at;
  std::optional<Uuid> deleted_by;
  [[nodiscard]] bool deleted() const noexcept { return deleted_at.has_value(); }
};

[[nodiscard]] ValidationErrors validate_comment_body(std::string_view body);

} // namespace taskflow::domain
