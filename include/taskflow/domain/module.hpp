#pragma once

#include <string_view>

namespace taskflow::domain {

[[nodiscard]] std::string_view module_name() noexcept;

} // namespace taskflow::domain
