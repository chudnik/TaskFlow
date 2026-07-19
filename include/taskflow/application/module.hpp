#pragma once

#include <string_view>

namespace taskflow::application {

[[nodiscard]] std::string_view module_name() noexcept;

} // namespace taskflow::application
