#include "taskflow/transport/websocket/module.hpp"

namespace taskflow::transport::websocket {

std::string_view module_name() noexcept { return "websocket_transport"; }

} // namespace taskflow::transport::websocket
