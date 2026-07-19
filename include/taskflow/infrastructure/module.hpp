#pragma once

#include <string_view>

namespace taskflow::infrastructure {

[[nodiscard]] std::string_view module_name() noexcept;

} // namespace taskflow::infrastructure
