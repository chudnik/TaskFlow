#include "taskflow/application/job_worker.hpp"
#include "taskflow/application/reminder_handlers.hpp"
#include "taskflow/infrastructure/job_repository.hpp"
#include "taskflow/infrastructure/notification_repository.hpp"
#include "taskflow/infrastructure/outbox_dispatcher.hpp"
#include "taskflow/infrastructure/outbox_repository.hpp"
#include "taskflow/infrastructure/postgres.hpp"
#include "taskflow/infrastructure/redis_notification_wakeup.hpp"
#include "taskflow/infrastructure/reminder_effect_repository.hpp"
#include "taskflow/infrastructure/schema_compatibility.hpp"
#include "taskflow/infrastructure/task_repositories.hpp"
#include "taskflow/platform/runtime_config.hpp"
#include "taskflow/platform/structured_logger.hpp"
#include "taskflow/service/runtime.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <string_view>

#if TASKFLOW_HAS_POSTGRES
#include <nlohmann/json.hpp>

namespace {

taskflow::application::ReminderRequest
parse_reminder(const taskflow::application::LeasedJob &job,
               const taskflow::application::ReminderKind kind) {
  try {
    const auto payload = nlohmann::json::parse(job.payload);
    const auto task_id = taskflow::domain::Uuid::parse(payload.at("task_id").get<std::string>());
    const auto assignee =
        taskflow::domain::Uuid::parse(payload.at("assignee_id").get<std::string>());
    auto deadline_value = payload.at("deadline_at").get<std::string>();
    if (deadline_value.ends_with("+00:00"))
      deadline_value.replace(deadline_value.size() - 6, 6, "Z");
    const auto deadline = taskflow::domain::parse_utc(deadline_value);
    if (!task_id || !assignee || !deadline)
      throw std::runtime_error{"reminder payload contains invalid identifiers"};
    const auto version = payload.at("version").get<std::uint64_t>();
    return {kind,      *task_id,  version,
            *assignee, *deadline, job.business_key + ":v" + std::to_string(version)};
  } catch (const nlohmann::json::exception &) {
    throw std::runtime_error{"reminder payload is invalid"};
  }
}

} // namespace
#endif

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
#if TASKFLOW_HAS_POSTGRES
    const auto schema = taskflow::infrastructure::check_postgres_schema(config.postgres_dsn);
    if (!schema.is_compatible()) {
      std::cerr << "schema compatibility error: " << schema.message() << '\n';
      return 3;
    }
    taskflow::service::WorkerRuntimeOwner owner{config};
    const auto &logger = owner.runtime().logger();
    auto &dependencies = owner.runtime().dependencies();
    auto &connection = dependencies.emplace_named<taskflow::infrastructure::PostgresConnection>(
        "postgres", config.postgres_dsn);
    auto &jobs = dependencies.emplace_named<taskflow::infrastructure::JobRepository>(
        "job-repository", connection);
    auto &tasks = dependencies.emplace_named<taskflow::infrastructure::TaskRepository>(
        "task-repository", connection);
    auto &effects = dependencies.emplace_named<taskflow::infrastructure::ReminderEffectRepository>(
        "reminder-effect-repository", connection);
    auto &clock = dependencies.emplace_named<taskflow::domain::SystemClock>("system-clock");
    auto &reminders = dependencies.emplace_named<taskflow::application::ReminderHandler>(
        "reminder-handler", tasks, effects, clock);
    auto &outbox = dependencies.emplace_named<taskflow::infrastructure::OutboxRepository>(
        "outbox-repository", connection);
    auto &notifications =
        dependencies.emplace_named<taskflow::infrastructure::NotificationRepository>(
            "notification-repository", connection);
    auto &redis = dependencies.emplace_named<taskflow::infrastructure::RedisNotificationWakeup>(
        "redis-notification-wakeup", config.redis_uri);
    auto &wakeups = dependencies.emplace_named<taskflow::application::WakeupCoordinator>(
        "wakeup-coordinator", redis);
    auto &outbox_dispatcher =
        dependencies.emplace_named<taskflow::infrastructure::OutboxDispatcher>(
            "outbox-dispatcher", outbox, notifications, wakeups, "taskflow-worker",
            config.worker_batch_size, std::chrono::seconds{config.worker_lease_seconds});
    auto &worker = dependencies.emplace_named<taskflow::application::JobWorker>("job-worker", jobs,
                                                                                "taskflow-worker");
    worker.register_handler("task.pre_deadline", [&reminders](const auto &job) {
      static_cast<void>(
          reminders.handle(parse_reminder(job, taskflow::application::ReminderKind::pre_deadline)));
    });
    worker.register_handler("task.overdue", [&reminders](const auto &job) {
      static_cast<void>(
          reminders.handle(parse_reminder(job, taskflow::application::ReminderKind::overdue)));
    });
    worker.register_cycle([&outbox_dispatcher] { return outbox_dispatcher.run_once(); });
    logger.log("info", taskflow::platform::CorrelationContext::job("startup", "startup"), "ready",
               0, "worker configuration loaded",
               {{"configuration", config.redacted_diagnostics(),
                 taskflow::platform::FieldSensitivity::public_value}});
    static_cast<void>(owner.runtime().start());
    worker.run_continuously(
        config.worker_batch_size, std::chrono::milliseconds{config.worker_poll_interval_ms},
        std::chrono::milliseconds{config.worker_retry_initial_ms},
        std::chrono::milliseconds{config.worker_retry_max_ms},
        [&](const auto duration) {
          if (stopping) {
            worker.request_stop();
            owner.runtime().request_stop();
            return true;
          }
          return owner.runtime().stop_controller().wait_for(duration);
        },
        [&](const std::string_view outcome, const std::string_view message) {
          logger.log(outcome == "dependency_retry" ? "warn" : "info",
                     taskflow::platform::CorrelationContext::job("worker-loop", "worker-loop"),
                     outcome, 0, std::string{message});
        });
    owner.runtime().request_stop();
    static_cast<void>(owner.runtime().finish());
#else
    static_cast<void>(config);
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
