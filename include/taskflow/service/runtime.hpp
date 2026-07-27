#pragma once

#include "taskflow/platform/runtime_config.hpp"
#include "taskflow/platform/runtime_lifecycle.hpp"
#include "taskflow/platform/structured_logger.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace taskflow::service {

class DependencyConstructionError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class DependencyOwner {
public:
  DependencyOwner() = default;
  ~DependencyOwner();
  DependencyOwner(const DependencyOwner &) = delete;
  DependencyOwner &operator=(const DependencyOwner &) = delete;
  DependencyOwner(DependencyOwner &&) noexcept;
  DependencyOwner &operator=(DependencyOwner &&) noexcept;

  template <typename Dependency, typename... Arguments>
  Dependency &emplace(Arguments &&...arguments) {
    auto dependency = std::make_unique<Owned<Dependency>>(std::forward<Arguments>(arguments)...);
    auto &result = dependency->value;
    dependencies_.push_back(std::move(dependency));
    return result;
  }

  template <typename Dependency, typename... Arguments>
  Dependency &emplace_named(const std::string &name, Arguments &&...arguments) {
    try {
      return emplace<Dependency>(std::forward<Arguments>(arguments)...);
    } catch (const std::exception &) {
      throw DependencyConstructionError{"unable to construct runtime dependency: " + name};
    }
  }

  [[nodiscard]] std::size_t size() const noexcept;
  void clear() noexcept;

private:
  struct Erased {
    virtual ~Erased() = default;
  };

  template <typename Dependency> struct Owned final : Erased {
    template <typename... Arguments>
    explicit Owned(Arguments &&...arguments) : value(std::forward<Arguments>(arguments)...) {}

    Dependency value;
  };

  std::vector<std::unique_ptr<Erased>> dependencies_;
};

class RuntimeOwner {
public:
  RuntimeOwner(platform::RuntimeConfig config, std::string service_name);
  ~RuntimeOwner();
  RuntimeOwner(const RuntimeOwner &) = delete;
  RuntimeOwner &operator=(const RuntimeOwner &) = delete;
  RuntimeOwner(RuntimeOwner &&) = delete;
  RuntimeOwner &operator=(RuntimeOwner &&) = delete;

  [[nodiscard]] const platform::RuntimeConfig &config() const noexcept;
  [[nodiscard]] const platform::StructuredLogger &logger() const noexcept;
  [[nodiscard]] platform::StopController &stop_controller() noexcept;
  [[nodiscard]] const platform::Lifecycle &lifecycle() const noexcept;
  [[nodiscard]] DependencyOwner &dependencies() noexcept;

  [[nodiscard]] bool start() noexcept;
  void request_stop() noexcept;
  [[nodiscard]] bool finish() noexcept;

private:
  platform::RuntimeConfig config_;
  platform::StructuredLogger logger_;
  platform::StopController stop_;
  platform::Lifecycle lifecycle_;
  DependencyOwner dependencies_;
};

class ApiRuntimeOwner {
public:
  explicit ApiRuntimeOwner(platform::RuntimeConfig config);
  [[nodiscard]] RuntimeOwner &runtime() noexcept;

private:
  RuntimeOwner runtime_;
};

class WorkerRuntimeOwner {
public:
  explicit WorkerRuntimeOwner(platform::RuntimeConfig config);
  [[nodiscard]] RuntimeOwner &runtime() noexcept;

private:
  RuntimeOwner runtime_;
};

} // namespace taskflow::service
