#include "taskflow/application/module.hpp"
#include "taskflow/infrastructure/schema_compatibility.hpp"
#include "taskflow/platform/runtime_config.hpp"
#include "taskflow/platform/structured_logger.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <string_view>

int main(int argc, char *argv[]) {
  if (argc == 2 && std::string_view{argv[1]} == "--version") {
    std::cout << "taskflow-worker 0.1.0\n";
    return 0;
  }

  try {
    static std::atomic_bool stopping{false};
    std::signal(SIGTERM, [](int) { stopping = true; });
    std::signal(SIGINT, [](int) { stopping = true; });
    const auto config = taskflow::platform::RuntimeConfig::from_environment();
    const auto schema = taskflow::infrastructure::check_postgres_schema(config.postgres_dsn);
    if (!schema.is_compatible()) {
      std::cerr << "schema compatibility error: " << schema.message() << '\n';
      return 3;
    }
    const taskflow::platform::StructuredLogger logger{"taskflow-worker", config.log_level};
    logger.log("info", taskflow::platform::CorrelationContext::job("startup", "startup"), "ready",
               0, "worker configuration loaded",
               {{"configuration", config.redacted_diagnostics(),
                 taskflow::platform::FieldSensitivity::public_value}});
    if (stopping)
      logger.log("info", taskflow::platform::CorrelationContext::job("shutdown", "shutdown"),
                 "stopped", 0, "worker shutdown requested");
    return 0;
  } catch (const taskflow::platform::ConfigError &error) {
    std::cerr << "configuration error: " << error.what() << '\n';
    return 2;
  } catch (const std::exception &error) {
    std::cerr << "startup dependency error: " << error.what() << '\n';
    return 3;
  }
}
