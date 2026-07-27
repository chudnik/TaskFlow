#include "taskflow/service/runtime.hpp"

namespace taskflow::service {

DependencyOwner::~DependencyOwner() { clear(); }
DependencyOwner::DependencyOwner(DependencyOwner &&) noexcept = default;
DependencyOwner &DependencyOwner::operator=(DependencyOwner &&) noexcept = default;

std::size_t DependencyOwner::size() const noexcept { return dependencies_.size(); }

void DependencyOwner::clear() noexcept {
  while (!dependencies_.empty()) {
    dependencies_.pop_back();
  }
}

RuntimeOwner::RuntimeOwner(platform::RuntimeConfig config, std::string service_name)
    : config_(std::move(config)), logger_(std::move(service_name), config_.log_level) {}

RuntimeOwner::~RuntimeOwner() {
  request_stop();
  static_cast<void>(finish());
}

const platform::RuntimeConfig &RuntimeOwner::config() const noexcept { return config_; }

const platform::StructuredLogger &RuntimeOwner::logger() const noexcept { return logger_; }

platform::StopController &RuntimeOwner::stop_controller() noexcept { return stop_; }

const platform::Lifecycle &RuntimeOwner::lifecycle() const noexcept { return lifecycle_; }

DependencyOwner &RuntimeOwner::dependencies() noexcept { return dependencies_; }

bool RuntimeOwner::start() noexcept { return lifecycle_.mark_running(); }

void RuntimeOwner::request_stop() noexcept {
  stop_.request_stop();
  static_cast<void>(lifecycle_.request_stop());
}

bool RuntimeOwner::finish() noexcept {
  request_stop();
  dependencies_.clear();
  return lifecycle_.mark_stopped();
}

ApiRuntimeOwner::ApiRuntimeOwner(platform::RuntimeConfig config)
    : runtime_(std::move(config), "taskflow-api") {}

RuntimeOwner &ApiRuntimeOwner::runtime() noexcept { return runtime_; }

WorkerRuntimeOwner::WorkerRuntimeOwner(platform::RuntimeConfig config)
    : runtime_(std::move(config), "taskflow-worker") {}

RuntimeOwner &WorkerRuntimeOwner::runtime() noexcept { return runtime_; }

} // namespace taskflow::service
