#include "taskflow/application/job_worker.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow;

class Jobs final : public application::JobLeaseStore {
public:
  std::vector<application::LeasedJob> leased;
  std::size_t completed{0};
  std::size_t retried{0};
  std::vector<application::LeasedJob> lease_jobs(std::string, std::size_t,
                                                 std::chrono::seconds) override {
    return std::exchange(leased, {});
  }
  void complete_job(const domain::Uuid &, std::string_view) override { ++completed; }
  void retry_job(const domain::Uuid &, std::string_view, std::string,
                 std::chrono::seconds) override {
    ++retried;
  }
};

TEST(JobWorkerTest, DispatchesRetriesLogsAndStopsGracefully) {
  Jobs jobs;
  jobs.leased = {{domain::Uuid::generate(), "a", "ok", "{}", 1, 3, "corr-a"},
                 {domain::Uuid::generate(), "b", "fail", "{}", 1, 3, "corr-b"}};
  std::size_t logs = 0;
  application::JobWorker worker{jobs, "worker-a",
                                [&](const auto &, std::string_view, std::string_view) { ++logs; }};
  worker.register_handler("ok", [](const auto &) {});
  worker.register_handler("fail", [](const auto &) { throw std::runtime_error{"transient"}; });
  EXPECT_EQ(worker.run_once(), 2U);
  EXPECT_EQ(jobs.completed, 1U);
  EXPECT_EQ(jobs.retried, 1U);
  EXPECT_EQ(logs, 2U);
  worker.request_stop();
  EXPECT_EQ(worker.run_once(), 0U);
}

} // namespace
