#include "taskflow/domain/module.hpp"
#include "taskflow/platform/runtime_config.hpp"
#include "taskflow/platform/structured_logger.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char *argv[]) {
  if (argc == 2 && std::string_view{argv[1]} == "--version") {
    std::cout << "taskflow-api 0.1.0\n";
    return 0;
  }

  try {
    const auto config = taskflow::platform::RuntimeConfig::from_environment();
    const taskflow::platform::StructuredLogger logger{"taskflow-api", config.log_level};
    logger.log("info", taskflow::platform::CorrelationContext::request("startup", "startup"),
               "ready", 0, "service configuration loaded",
               {{"configuration", config.redacted_diagnostics(),
                 taskflow::platform::FieldSensitivity::public_value}});
    return 0;
  } catch (const taskflow::platform::ConfigError &error) {
    std::cerr << "configuration error: " << error.what() << '\n';
    return 2;
  }
}
