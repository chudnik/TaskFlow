#pragma once

#include "taskflow/application/job_worker.hpp"
#include "taskflow/application/reminders.hpp"
#include "taskflow/domain/common.hpp"
#include "taskflow/infrastructure/postgres.hpp"

namespace taskflow::infrastructure {

struct Job {
  domain::Uuid id;
  std::string business_key;
  std::string type;
  std::string payload;
  std::size_t attempt;
  std::size_t maximum_attempts;
  std::string correlation_id;
};

class JobRepository final : public application::JobLeaseStore,
                            public application::ReminderJobStore {
public:
  explicit JobRepository(PostgresConnection &connection);
  [[nodiscard]] domain::Uuid schedule(std::string business_key, std::string type,
                                      std::string payload, domain::UtcInstant scheduled_at,
                                      std::size_t maximum_attempts, std::string correlation_id);
  [[nodiscard]] std::vector<Job> lease_due(std::string worker_id, std::size_t batch_size,
                                           std::chrono::seconds lease_duration);
  void succeed(const domain::Uuid &job_id, std::string_view worker_id);
  void fail(const domain::Uuid &job_id, std::string_view worker_id, std::string error,
            std::chrono::seconds backoff);
  void cancel_business_key(std::string_view business_key);
  [[nodiscard]] std::vector<application::LeasedJob>
  lease_jobs(std::string worker_id, std::size_t batch_size,
             std::chrono::seconds lease_duration) override;
  void complete_job(const domain::Uuid &job_id, std::string_view worker_id) override;
  void retry_job(const domain::Uuid &job_id, std::string_view worker_id, std::string error,
                 std::chrono::seconds backoff) override;
  void upsert_reminder(std::string business_key, std::string type, std::string payload,
                       domain::UtcInstant scheduled_at, std::string correlation_id) override;
  void cancel_reminder(std::string_view business_key) override;

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
