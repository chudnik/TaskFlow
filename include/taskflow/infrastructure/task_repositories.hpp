#pragma once

#include "taskflow/application/task_list.hpp"
#include "taskflow/application/tasks.hpp"
#include "taskflow/infrastructure/postgres.hpp"

#include <optional>
#include <string>

namespace taskflow::infrastructure {

class TaskRepository final : public application::TaskStore, public application::TaskQueryStore {
public:
  explicit TaskRepository(PostgresConnection &connection);

  [[nodiscard]] domain::Task create(const domain::Uuid &project_id, std::string title,
                                    std::string description, domain::TaskPriority priority,
                                    const domain::Uuid &creator_id,
                                    std::optional<domain::Uuid> assignee_id,
                                    std::optional<domain::UtcInstant> deadline_at);
  [[nodiscard]] domain::Task create_task(const domain::Uuid &project_id, std::string title,
                                         std::string description, domain::TaskPriority priority,
                                         const domain::Uuid &creator_id,
                                         std::optional<domain::Uuid> assignee_id,
                                         std::optional<domain::UtcInstant> deadline_at) override;
  [[nodiscard]] std::optional<domain::Task> find_active(const domain::Uuid &task_id);
  [[nodiscard]] std::optional<domain::Task> find_active_task(const domain::Uuid &task_id) override;
  [[nodiscard]] domain::Task update(const domain::Task &task, std::uint64_t expected_version);
  [[nodiscard]] domain::Task update_task(const domain::Task &task,
                                         std::uint64_t expected_version) override;
  [[nodiscard]] domain::Task soft_delete(const domain::Uuid &task_id,
                                         std::uint64_t expected_version,
                                         const domain::Uuid &actor_id);
  [[nodiscard]] domain::Task delete_task(const domain::Uuid &task_id,
                                         std::uint64_t expected_version,
                                         const domain::Uuid &actor_id) override;
  [[nodiscard]] std::vector<domain::Task> list_tasks(const application::NormalizedTaskQuery &query,
                                                     const domain::Uuid &caller_id,
                                                     std::optional<application::TaskCursor> after,
                                                     std::size_t limit,
                                                     domain::UtcInstant now) override;

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
