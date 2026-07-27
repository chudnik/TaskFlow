#include "taskflow/application/comments.hpp"

#include <utility>

namespace taskflow::application {

CommentUseCases::CommentUseCases(ProjectStore &projects, TaskStore &tasks, CommentStore &comments)
    : projects_{&projects}, tasks_{&tasks}, comments_{&comments} {}

domain::Task CommentUseCases::require_visible_task(const AuthenticatedPrincipal &actor,
                                                   const domain::Uuid &task_id) const {
  const auto task = tasks_->find_active_task(task_id);
  if (!task || !projects_->find_visible_project(task->project_id, actor.user_id, false))
    throw TaskError{TaskErrorCode::not_found, "task not found"};
  return *task;
}

bool CommentUseCases::can_moderate(const AuthenticatedPrincipal &actor,
                                   const domain::Uuid &project_id) const {
  const auto role = projects_->find_role(project_id, actor.user_id);
  return role && (*role == domain::ProjectRole::owner || *role == domain::ProjectRole::manager);
}

domain::Comment CommentUseCases::create(const AuthenticatedPrincipal &actor,
                                        const domain::Uuid &task_id, std::string body) const {
  static_cast<void>(require_visible_task(actor, task_id));
  const auto errors = domain::validate_comment_body(body);
  if (!errors.empty())
    throw TaskError{TaskErrorCode::invalid_input, errors.items().front().message};
  return comments_->create_comment(task_id, actor.user_id, std::move(body));
}

std::vector<domain::Comment> CommentUseCases::list(const AuthenticatedPrincipal &actor,
                                                   const domain::Uuid &task_id) const {
  static_cast<void>(require_visible_task(actor, task_id));
  return comments_->list_comments(task_id);
}

domain::Comment CommentUseCases::edit(const AuthenticatedPrincipal &actor,
                                      const domain::Uuid &comment_id, std::string body) const {
  const auto comment = comments_->find_comment(comment_id);
  if (!comment)
    throw TaskError{TaskErrorCode::not_found, "comment not found"};
  const auto task = require_visible_task(actor, comment->task_id);
  if (comment->author_id != actor.user_id && !can_moderate(actor, task.project_id))
    throw TaskError{TaskErrorCode::forbidden, "comment action is forbidden"};
  const auto errors = domain::validate_comment_body(body);
  if (!errors.empty())
    throw TaskError{TaskErrorCode::invalid_input, errors.items().front().message};
  return comments_->update_comment(comment_id, std::move(body));
}

void CommentUseCases::remove(const AuthenticatedPrincipal &actor,
                             const domain::Uuid &comment_id) const {
  const auto comment = comments_->find_comment(comment_id);
  if (!comment)
    throw TaskError{TaskErrorCode::not_found, "comment not found"};
  const auto task = require_visible_task(actor, comment->task_id);
  if (comment->author_id != actor.user_id && !can_moderate(actor, task.project_id))
    throw TaskError{TaskErrorCode::forbidden, "comment action is forbidden"};
  comments_->delete_comment(comment_id, actor.user_id);
}

} // namespace taskflow::application
