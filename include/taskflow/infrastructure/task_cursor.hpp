#pragma once

#include "taskflow/application/task_cursor.hpp"

#include <string>

namespace taskflow::infrastructure {

class SignedTaskCursorCodec final : public application::TaskCursorCodec {
public:
  explicit SignedTaskCursorCodec(std::string secret);
  [[nodiscard]] std::string encode(const application::TaskCursor &cursor) const override;
  [[nodiscard]] application::TaskCursor
  decode(std::string_view encoded, std::string_view expected_fingerprint) const override;

private:
  std::string secret_;
};

} // namespace taskflow::infrastructure
