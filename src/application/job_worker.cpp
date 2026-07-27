#include "taskflow/application/job_worker.hpp"

namespace taskflow::application {

JobWorker::JobWorker(JobLeaseStore &jobs, std::string worker_id, JobLog log)
    : jobs_{&jobs}, worker_id_{std::move(worker_id)}, log_{std::move(log)} {}

void JobWorker::register_handler(std::string type, JobHandler handler) {
  handlers_.insert_or_assign(std::move(type), std::move(handler));
}

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

void JobWorker::request_stop() noexcept { stopping_ = true; }
bool JobWorker::stopping() const noexcept { return stopping_; }
void JobWorker::wake() noexcept { wake_requested_ = true; }

} // namespace taskflow::application
