#include "taskflow/transport/http/module.hpp"

namespace taskflow::transport::http {

std::string_view module_name() noexcept { return "http_transport"; }

} // namespace taskflow::transport::http
