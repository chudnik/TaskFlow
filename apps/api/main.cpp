#include "taskflow/domain/module.hpp"
#include "taskflow/infrastructure/schema_compatibility.hpp"
#include "taskflow/platform/runtime_config.hpp"
#include "taskflow/platform/structured_logger.hpp"
#include "taskflow/transport/http/api_router.hpp"

#if TASKFLOW_HAS_DROGON
#include <drogon/HttpAppFramework.h>
#endif

#include <csignal>
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
    try {
      const auto schema = taskflow::infrastructure::check_postgres_schema(config.postgres_dsn);
      if (!schema.is_compatible()) {
        std::cerr << "schema compatibility error: " << schema.message() << '\n';
        return 3;
      }
    } catch (const std::exception &) {
      logger.log("warn", taskflow::platform::CorrelationContext::request("startup", "startup"),
                 "degraded", 0, "PostgreSQL unavailable during startup; readiness disabled");
    }
    logger.log("info", taskflow::platform::CorrelationContext::request("startup", "startup"),
               "ready", 0, "service configuration loaded",
               {{"configuration", config.redacted_diagnostics(),
                 taskflow::platform::FieldSensitivity::public_value}});
#if TASKFLOW_HAS_DROGON
    std::signal(SIGTERM, [](int) { drogon::app().quit(); });
    std::signal(SIGINT, [](int) { drogon::app().quit(); });
    auto &application = drogon::app();
    taskflow::transport::http::configure_api_router(application, [dsn = config.postgres_dsn]() {
      try {
        const auto schema = taskflow::infrastructure::check_postgres_schema(dsn);
        return taskflow::transport::http::ReadinessReport{schema.is_compatible(), "available",
                                                          schema.is_compatible() ? "compatible"
                                                                                 : "incompatible"};
      } catch (const std::exception &) {
        return taskflow::transport::http::ReadinessReport{false, "unavailable", "unknown"};
      }
    });
    application.setIdleConnectionTimeout(config.http_idle_timeout_seconds)
        .addListener(config.http_address, config.http_port)
        .run();
#endif
    return 0;
  } catch (const taskflow::platform::ConfigError &error) {
    std::cerr << "configuration error: " << error.what() << '\n';
    return 2;
  } catch (const std::exception &error) {
    std::cerr << "startup dependency error: " << error.what() << '\n';
    return 3;
  }
}
