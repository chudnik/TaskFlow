#include "taskflow/application/task_list.hpp"
#include "taskflow/application/tasks.hpp"

#include <algorithm>
#include <cctype>

namespace taskflow::application {

TaskListUseCase::TaskListUseCase(ProjectStore &projects, TaskQueryStore &tasks,
                                 const TaskCursorCodec &cursors, const domain::Clock &clock)
    : projects_{&projects}, tasks_{&tasks}, cursors_{&cursors}, clock_{&clock} {}

std::optional<std::string> TaskListUseCase::sort_value(const domain::Task &task,
                                                       const TaskSortField field) {
  switch (field) {
  case TaskSortField::created_at:
    return domain::format_utc(task.created_at);
  case TaskSortField::updated_at:
    return domain::format_utc(task.updated_at);
  case TaskSortField::deadline:
    return task.deadline_at ? std::optional{domain::format_utc(*task.deadline_at)} : std::nullopt;
  case TaskSortField::priority:
    return std::to_string(static_cast<int>(task.priority));
  case TaskSortField::title: {
    std::string title = task.title;
    std::transform(title.begin(), title.end(), title.begin(), [](const unsigned char value) {
      return static_cast<char>(std::tolower(value));
    });
    return title;
  }
  }
  return std::nullopt;
}

TaskPage TaskListUseCase::list(const AuthenticatedPrincipal &actor, NormalizedTaskQuery query,
                               const domain::PageRequest page) const {
  if (!projects_->find_visible_project(query.filters.project_id, actor.user_id,
                                       actor.global_role == domain::GlobalRole::admin)) {
    throw TaskError{TaskErrorCode::not_found, "project not found"};
  }
  if (!projects_->find_role(query.filters.project_id, actor.user_id)) {
    throw TaskError{TaskErrorCode::forbidden, "task listing requires project membership"};
  }
  const auto fingerprint = task_query_fingerprint(query);
  const auto after =
      page.cursor ? std::optional{cursors_->decode(*page.cursor, fingerprint)} : std::nullopt;
  auto items = tasks_->list_tasks(query, actor.user_id, after, page.size + 1, clock_->now());
  std::optional<std::string> next;
  if (items.size() > page.size) {
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(page.size), items.end());
    const auto &last = items.back();
    next = cursors_->encode({1, fingerprint, sort_value(last, query.order.field), last.id});
  }
  return {std::move(items), std::move(next)};
}

} // namespace taskflow::application
