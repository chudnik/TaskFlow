#pragma once

#include <string_view>

namespace taskflow::transport::http {

[[nodiscard]] std::string_view module_name() noexcept;

} // namespace taskflow::transport::http
