#pragma once

#include "taskflow/domain/common.hpp"

#include <atomic>
#include <functional>
#include <map>

namespace taskflow::application {

struct LeasedJob {
  domain::Uuid id;
  std::string business_key;
  std::string type;
  std::string payload;
  std::size_t attempt;
  std::size_t maximum_attempts;
  std::string correlation_id;
};

class JobLeaseStore {
public:
  virtual ~JobLeaseStore() = default;
  [[nodiscard]] virtual std::vector<LeasedJob> lease_jobs(std::string worker_id,
                                                          std::size_t batch_size,
                                                          std::chrono::seconds lease_duration) = 0;
  virtual void complete_job(const domain::Uuid &job_id, std::string_view worker_id) = 0;
  virtual void retry_job(const domain::Uuid &job_id, std::string_view worker_id, std::string error,
                         std::chrono::seconds backoff) = 0;
};

using JobHandler = std::function<void(const LeasedJob &)>;
using JobLog =
    std::function<void(const LeasedJob &, std::string_view outcome, std::string_view message)>;
using WorkerWait = std::function<bool(std::chrono::milliseconds)>;
using WorkerLog = std::function<void(std::string_view outcome, std::string_view message)>;
using WorkerCycle = std::function<std::size_t()>;

class JobWorker {
public:
  JobWorker(JobLeaseStore &jobs, std::string worker_id, JobLog log = {});
  void register_handler(std::string type, JobHandler handler);
  void register_cycle(WorkerCycle cycle);
  [[nodiscard]] std::size_t run_once(std::size_t batch_size = 16);
  void run_continuously(std::size_t batch_size, std::chrono::milliseconds poll_interval,
                        std::chrono::milliseconds retry_initial,
                        std::chrono::milliseconds retry_max, WorkerWait wait, WorkerLog log = {});
  void request_stop() noexcept;
  [[nodiscard]] bool stopping() const noexcept;
  void wake() noexcept;

private:
  JobLeaseStore *jobs_;
  std::string worker_id_;
  JobLog log_;
  std::map<std::string, JobHandler> handlers_;
  std::vector<WorkerCycle> cycles_;
  std::atomic_bool stopping_{false};
  std::atomic_bool wake_requested_{false};
};

} // namespace taskflow::application
