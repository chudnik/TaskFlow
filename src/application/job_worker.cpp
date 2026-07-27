#include "taskflow/application/job_worker.hpp"

#include <algorithm>

namespace taskflow::application {

JobWorker::JobWorker(JobLeaseStore &jobs, std::string worker_id, JobLog log)
    : jobs_{&jobs}, worker_id_{std::move(worker_id)}, log_{std::move(log)} {}

void JobWorker::register_handler(std::string type, JobHandler handler) {
  handlers_.insert_or_assign(std::move(type), std::move(handler));
}

void JobWorker::register_cycle(WorkerCycle cycle) { cycles_.push_back(std::move(cycle)); }

std::size_t JobWorker::run_once(const std::size_t batch_size) {
  if (stopping_)
    return 0;
  wake_requested_ = false;
  auto jobs = jobs_->lease_jobs(worker_id_, batch_size, std::chrono::seconds{30});
  for (const auto &job : jobs) {
    const auto handler = handlers_.find(job.type);
    try {
      if (handler == handlers_.end())
        throw std::runtime_error{"job handler is not registered"};
      handler->second(job);
      jobs_->complete_job(job.id, worker_id_);
      if (log_)
        log_(job, "succeeded", "");
    } catch (const std::exception &error) {
      const auto exponent = std::min<std::size_t>(job.attempt, 8);
      jobs_->retry_job(job.id, worker_id_, error.what(), std::chrono::seconds{1ULL << exponent});
      if (log_)
        log_(job, job.attempt >= job.maximum_attempts ? "failed" : "retry", error.what());
    }
  }
  return jobs.size();
}

void JobWorker::run_continuously(const std::size_t batch_size,
                                 const std::chrono::milliseconds poll_interval,
                                 const std::chrono::milliseconds retry_initial,
                                 const std::chrono::milliseconds retry_max, WorkerWait wait,
                                 WorkerLog log) {
  auto retry_delay = retry_initial;
  while (!stopping_) {
    try {
      auto processed = run_once(batch_size);
      for (const auto &cycle : cycles_)
        processed += cycle();
      retry_delay = retry_initial;
      if (log)
        log("cycle_completed", std::to_string(processed));
      if (processed == 0 && wait(poll_interval))
        request_stop();
    } catch (const std::exception &error) {
      if (log)
        log("dependency_retry", error.what());
      if (wait(retry_delay))
        request_stop();
      retry_delay = std::min(retry_delay * 2, retry_max);
    }
  }
  if (log)
    log("stopped", "");
}

void JobWorker::request_stop() noexcept { stopping_ = true; }
bool JobWorker::stopping() const noexcept { return stopping_; }
void JobWorker::wake() noexcept { wake_requested_ = true; }

} // namespace taskflow::application
