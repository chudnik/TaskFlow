#pragma once

#include "taskflow/application/projects.hpp"
#include "taskflow/domain/task.hpp"

#include <optional>
#include <stdexcept>
#include <string>

namespace taskflow::application {

class ReminderScheduler;

enum class TaskErrorCode {
  invalid_input,
  not_found,
  forbidden,
  archived,
  conflict,
  invalid_transition,
  invalid_assignee
};

class TaskError : public std::runtime_error {
public:
  TaskError(TaskErrorCode code, std::string message,
            std::optional<domain::Task> current = std::nullopt);
  [[nodiscard]] TaskErrorCode code() const noexcept;
  [[nodiscard]] const std::optional<domain::Task> &current() const noexcept;

private:
  TaskErrorCode code_;
  std::optional<domain::Task> current_;
};

class TaskStore {
public:
  virtual ~TaskStore() = default;
  [[nodiscard]] virtual domain::Task create_task(const domain::Uuid &project_id, std::string title,
                                                 std::string description,
                                                 domain::TaskPriority priority,
                                                 const domain::Uuid &creator_id,
                                                 std::optional<domain::Uuid> assignee_id,
                                                 std::optional<domain::UtcInstant> deadline_at) = 0;
  [[nodiscard]] virtual std::optional<domain::Task>
  find_active_task(const domain::Uuid &task_id) = 0;
  [[nodiscard]] virtual domain::Task update_task(const domain::Task &task,
                                                 std::uint64_t expected_version) = 0;
  [[nodiscard]] virtual domain::Task delete_task(const domain::Uuid &task_id,
                                                 std::uint64_t expected_version,
                                                 const domain::Uuid &actor_id) = 0;
};

struct CreateTaskInput {
  domain::Uuid project_id;
  std::string title;
  std::string description;
  domain::TaskPriority priority{domain::TaskPriority::medium};
  std::optional<domain::Uuid> assignee_id;
  std::optional<domain::UtcInstant> deadline_at;
};

struct UpdateTaskInput {
  std::string title;
  std::string description;
  domain::TaskPriority priority;
  std::optional<domain::UtcInstant> deadline_at;
  std::uint64_t version;
};

class TaskUseCases {
public:
  TaskUseCases(ProjectStore &projects, TaskStore &tasks, const PolicyService &policy,
               const domain::Clock &clock);
  TaskUseCases(ProjectStore &projects, TaskStore &tasks, const PolicyService &policy,
               const domain::Clock &clock, const ReminderScheduler &reminders);

  [[nodiscard]] domain::Task create(const AuthenticatedPrincipal &actor,
                                    CreateTaskInput input) const;
  [[nodiscard]] domain::Task read(const AuthenticatedPrincipal &actor,
                                  const domain::Uuid &task_id) const;
  [[nodiscard]] domain::Task update(const AuthenticatedPrincipal &actor,
                                    const domain::Uuid &task_id, UpdateTaskInput input) const;
  [[nodiscard]] domain::Task transition(const AuthenticatedPrincipal &actor,
                                        const domain::Uuid &task_id, domain::TaskStatus status,
                                        std::uint64_t version) const;
  [[nodiscard]] domain::Task assign(const AuthenticatedPrincipal &actor,
                                    const domain::Uuid &task_id,
                                    std::optional<domain::Uuid> assignee_id,
                                    std::uint64_t version) const;
  void remove(const AuthenticatedPrincipal &actor, const domain::Uuid &task_id,
              std::uint64_t version) const;

private:
  [[nodiscard]] domain::Task authorize(const AuthenticatedPrincipal &actor,
                                       const domain::Uuid &task_id, TaskAction action) const;
  void require_member(const AuthenticatedPrincipal &actor, const domain::Uuid &project_id) const;
  void require_current(const domain::Task &task, std::uint64_t version) const;

  ProjectStore *projects_;
  TaskStore *tasks_;
  const PolicyService *policy_;
  const domain::Clock *clock_;
  const ReminderScheduler *reminders_{nullptr};
};

} // namespace taskflow::application
