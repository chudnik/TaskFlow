#pragma once

#include "taskflow/application/projects.hpp"
#include "taskflow/application/task_cursor.hpp"
#include "taskflow/application/task_query.hpp"

namespace taskflow::application {

class TaskQueryStore {
public:
  virtual ~TaskQueryStore() = default;
  [[nodiscard]] virtual std::vector<domain::Task>
  list_tasks(const NormalizedTaskQuery &query, const domain::Uuid &caller_id,
             std::optional<TaskCursor> after, std::size_t limit, domain::UtcInstant now) = 0;
};

struct TaskPage {
  std::vector<domain::Task> items;
  std::optional<std::string> next_cursor;
};

class TaskListUseCase {
public:
  TaskListUseCase(ProjectStore &projects, TaskQueryStore &tasks, const TaskCursorCodec &cursors,
                  const domain::Clock &clock);
  [[nodiscard]] TaskPage list(const AuthenticatedPrincipal &actor, NormalizedTaskQuery query,
                              domain::PageRequest page) const;

private:
  [[nodiscard]] static std::optional<std::string> sort_value(const domain::Task &task,
                                                             TaskSortField field);
  ProjectStore *projects_;
  TaskQueryStore *tasks_;
  const TaskCursorCodec *cursors_;
  const domain::Clock *clock_;
};

} // namespace taskflow::application
