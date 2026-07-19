#pragma once

#include <string_view>

namespace taskflow::transport::websocket {

[[nodiscard]] std::string_view module_name() noexcept;

} // namespace taskflow::transport::websocket
