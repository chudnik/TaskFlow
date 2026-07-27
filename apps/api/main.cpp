#include "taskflow/application/audit.hpp"
#include "taskflow/application/authentication_middleware.hpp"
#include "taskflow/application/authentication_sessions.hpp"
#include "taskflow/application/authorization.hpp"
#include "taskflow/application/comments.hpp"
#include "taskflow/application/identity.hpp"
#include "taskflow/application/task_list.hpp"
#include "taskflow/application/tasks.hpp"
#include "taskflow/domain/module.hpp"
#include "taskflow/infrastructure/audit_repository.hpp"
#include "taskflow/infrastructure/authentication_sessions.hpp"
#include "taskflow/infrastructure/comment_repositories.hpp"
#include "taskflow/infrastructure/identity_repositories.hpp"
#include "taskflow/infrastructure/jwt_access_token.hpp"
#include "taskflow/infrastructure/notification_delivery.hpp"
#include "taskflow/infrastructure/notification_repository.hpp"
#include "taskflow/infrastructure/password_hasher.hpp"
#include "taskflow/infrastructure/postgres.hpp"
#include "taskflow/infrastructure/project_repositories.hpp"
#include "taskflow/infrastructure/redis_notification_subscriber.hpp"
#include "taskflow/infrastructure/refresh_tokens.hpp"
#include "taskflow/infrastructure/schema_compatibility.hpp"
#include "taskflow/infrastructure/task_cursor.hpp"
#include "taskflow/infrastructure/task_repositories.hpp"
#include "taskflow/platform/runtime_config.hpp"
#include "taskflow/platform/structured_logger.hpp"
#include "taskflow/service/runtime.hpp"
#include "taskflow/transport/http/api_router.hpp"
#include "taskflow/transport/http/audit_controller.hpp"
#include "taskflow/transport/http/comment_controller.hpp"
#include "taskflow/transport/http/identity_controller.hpp"
#include "taskflow/transport/http/membership_controller.hpp"
#include "taskflow/transport/http/project_controller.hpp"
#include "taskflow/transport/http/task_controller.hpp"
#include "taskflow/transport/websocket/gateway.hpp"
#include "taskflow/transport/websocket/runtime_controller.hpp"

#if TASKFLOW_HAS_DROGON
#include <drogon/HttpAppFramework.h>
#endif

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
#if TASKFLOW_HAS_DROGON
std::atomic_bool shutdown_requested{false};
std::atomic_bool accepting_requests{true};
#endif
} // namespace

int main(int argc, char *argv[]) {
  if (argc == 2 && std::string_view{argv[1]} == "--version") {
    std::cout << "taskflow-api 0.1.0\n";
    return 0;
  }

  try {
    const auto config = taskflow::platform::RuntimeConfig::from_environment();
    taskflow::service::ApiRuntimeOwner owner{config};
    const auto &logger = owner.runtime().logger();
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
    auto &dependencies = owner.runtime().dependencies();
    auto &clock = dependencies.emplace_named<taskflow::domain::SystemClock>("system-clock");
    auto &connection = dependencies.emplace_named<taskflow::infrastructure::PostgresConnection>(
        "postgres", config.postgres_dsn);
    auto &users = dependencies.emplace_named<taskflow::infrastructure::UserRepository>(
        "user-repository", connection);
    auto &sessions = dependencies.emplace_named<taskflow::infrastructure::SessionRepository>(
        "session-repository", connection);
    auto &passwords =
        dependencies.emplace_named<taskflow::infrastructure::PasswordHasher>("password-hasher");
    auto &refresh_tokens =
        dependencies.emplace_named<taskflow::infrastructure::RefreshTokenService>(
            "refresh-token-service", connection);
    auto &session_store =
        dependencies.emplace_named<taskflow::infrastructure::PostgresAuthenticationSessionStore>(
            "authentication-session-store", refresh_tokens, sessions, users);
    auto &session_validator =
        dependencies.emplace_named<taskflow::infrastructure::PostgresAccountSessionValidator>(
            "account-session-validator", connection);
    auto &access_tokens =
        dependencies.emplace_named<taskflow::infrastructure::JwtAccessTokenService>(
            "access-token-service", config.jwt_signing_secret, config.jwt_issuer,
            config.jwt_audience, taskflow::application::access_token_lifetime, clock,
            session_validator);
    auto &identity = dependencies.emplace_named<taskflow::application::IdentityUseCases>(
        "identity-use-cases", users, passwords);
    auto &authentication_sessions =
        dependencies.emplace_named<taskflow::application::AuthenticationSessionUseCases>(
            "authentication-session-use-cases", identity, session_store, access_tokens, clock);
    auto &authentication =
        dependencies.emplace_named<taskflow::application::AuthenticationMiddleware>(
            "authentication-middleware", access_tokens);
    auto &authorization =
        dependencies.emplace_named<taskflow::application::PolicyService>("authorization-policy");
    auto &identity_controller =
        dependencies.emplace_named<taskflow::transport::http::IdentityController>(
            "identity-controller", authentication_sessions, authentication);
    auto &projects = dependencies.emplace_named<taskflow::infrastructure::ProjectRepository>(
        "project-repository", connection);
    auto &memberships =
        dependencies.emplace_named<taskflow::infrastructure::ProjectMembershipRepository>(
            "membership-repository", connection);
    auto &project_use_cases = dependencies.emplace_named<taskflow::application::ProjectUseCases>(
        "project-use-cases", projects, authorization);
    auto &membership_use_cases =
        dependencies.emplace_named<taskflow::application::MembershipUseCases>(
            "membership-use-cases", projects, memberships, authorization);
    auto &project_controller =
        dependencies.emplace_named<taskflow::transport::http::ProjectController>(
            "project-controller", project_use_cases);
    auto &membership_controller =
        dependencies.emplace_named<taskflow::transport::http::MembershipController>(
            "membership-controller", membership_use_cases);
    auto &tasks = dependencies.emplace_named<taskflow::infrastructure::TaskRepository>(
        "task-repository", connection);
    auto &comments = dependencies.emplace_named<taskflow::infrastructure::CommentRepository>(
        "comment-repository", connection);
    auto &audit = dependencies.emplace_named<taskflow::infrastructure::AuditRepository>(
        "audit-repository", connection);
    auto &task_cursors =
        dependencies.emplace_named<taskflow::infrastructure::SignedTaskCursorCodec>(
            "task-cursor-codec", config.jwt_signing_secret);
    auto &task_use_cases = dependencies.emplace_named<taskflow::application::TaskUseCases>(
        "task-use-cases", projects, tasks, authorization, clock);
    auto &task_list_use_case = dependencies.emplace_named<taskflow::application::TaskListUseCase>(
        "task-list-use-case", projects, tasks, task_cursors, clock);
    auto &comment_use_cases = dependencies.emplace_named<taskflow::application::CommentUseCases>(
        "comment-use-cases", projects, tasks, comments);
    auto &audit_use_cases = dependencies.emplace_named<taskflow::application::AuditUseCases>(
        "audit-use-cases", projects, audit);
    auto &task_controller = dependencies.emplace_named<taskflow::transport::http::TaskController>(
        "task-controller", task_use_cases, task_list_use_case);
    auto &comment_controller =
        dependencies.emplace_named<taskflow::transport::http::CommentController>(
            "comment-controller", comment_use_cases);
    auto &audit_controller = dependencies.emplace_named<taskflow::transport::http::AuditController>(
        "audit-controller", audit_use_cases, tasks);
    auto &notifications =
        dependencies.emplace_named<taskflow::infrastructure::NotificationRepository>(
            "notification-repository", connection);
    auto &notification_delivery =
        dependencies.emplace_named<taskflow::infrastructure::NotificationDelivery>(
            "notification-delivery", notifications);
    auto &websocket_gateway = dependencies.emplace_named<taskflow::transport::websocket::Gateway>(
        "websocket-gateway", authentication, clock, std::size_t{256}, std::chrono::seconds{45},
        static_cast<std::size_t>(config.maximum_connections));
    auto &websocket_controller =
        dependencies
            .emplace_named<std::shared_ptr<taskflow::transport::websocket::RuntimeController>>(
                "websocket-controller",
                std::make_shared<taskflow::transport::websocket::RuntimeController>(
                    websocket_gateway, notification_delivery, projects, config.worker_batch_size));
    static_cast<void>(
        dependencies.emplace_named<taskflow::infrastructure::RedisNotificationSubscriber>(
            "redis-notification-subscriber", config.redis_uri,
            [weak = std::weak_ptr<taskflow::transport::websocket::RuntimeController>{
                 websocket_controller}] {
              drogon::app().getLoop()->queueInLoop([weak] {
                if (const auto controller = weak.lock())
                  controller->poll();
              });
            }));

    std::signal(SIGTERM, [](int) { shutdown_requested = true; });
    std::signal(SIGINT, [](int) { shutdown_requested = true; });
    auto &application = drogon::app();
    application.registerController(websocket_controller);
    taskflow::transport::http::configure_api_router(
        application,
        [dsn = config.postgres_dsn]() {
          try {
            const auto schema = taskflow::infrastructure::check_postgres_schema(dsn);
            return taskflow::transport::http::ReadinessReport{
                schema.is_compatible(), "available",
                schema.is_compatible() ? "compatible" : "incompatible"};
          } catch (const std::exception &) {
            return taskflow::transport::http::ReadinessReport{false, "unavailable", "unknown"};
          }
        },
        &identity_controller, &authentication, &project_controller, &membership_controller,
        &task_controller, &comment_controller, &audit_controller, &accepting_requests);
    static_cast<void>(owner.runtime().start());
    application.setThreadNum(1)
        .setIdleConnectionTimeout(config.http_idle_timeout_seconds)
        .addListener(config.http_address, config.http_port)
        .getLoop()
        ->runEvery(1.0, [&websocket_controller] { websocket_controller->poll(); });
    application.getLoop()->runEvery(0.1, [&owner, &websocket_controller] {
      if (!shutdown_requested.exchange(false))
        return;
      accepting_requests.store(false, std::memory_order_release);
      owner.runtime().request_stop();
      websocket_controller->shutdown();
      drogon::app().quit();
    });
    application.run();
    websocket_controller->shutdown();
    owner.runtime().request_stop();
    static_cast<void>(owner.runtime().finish());
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
