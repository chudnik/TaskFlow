#pragma once

#include "taskflow/application/tasks.hpp"
#include "taskflow/domain/comment.hpp"

namespace taskflow::application {

class CommentStore {
public:
  virtual ~CommentStore() = default;
  [[nodiscard]] virtual domain::Comment
  create_comment(const domain::Uuid &task_id, const domain::Uuid &author_id, std::string body) = 0;
  [[nodiscard]] virtual std::optional<domain::Comment>
  find_comment(const domain::Uuid &comment_id) = 0;
  [[nodiscard]] virtual std::vector<domain::Comment> list_comments(const domain::Uuid &task_id) = 0;
  [[nodiscard]] virtual domain::Comment update_comment(const domain::Uuid &comment_id,
                                                       std::string body) = 0;
  virtual void delete_comment(const domain::Uuid &comment_id, const domain::Uuid &actor_id) = 0;
};

class CommentUseCases {
public:
  CommentUseCases(ProjectStore &projects, TaskStore &tasks, CommentStore &comments);
  [[nodiscard]] domain::Comment create(const AuthenticatedPrincipal &actor,
                                       const domain::Uuid &task_id, std::string body) const;
  [[nodiscard]] std::vector<domain::Comment> list(const AuthenticatedPrincipal &actor,
                                                  const domain::Uuid &task_id) const;
  [[nodiscard]] domain::Comment edit(const AuthenticatedPrincipal &actor,
                                     const domain::Uuid &comment_id, std::string body) const;
  void remove(const AuthenticatedPrincipal &actor, const domain::Uuid &comment_id) const;

private:
  [[nodiscard]] domain::Task require_visible_task(const AuthenticatedPrincipal &actor,
                                                  const domain::Uuid &task_id) const;
  [[nodiscard]] bool can_moderate(const AuthenticatedPrincipal &actor,
                                  const domain::Uuid &project_id) const;
  ProjectStore *projects_;
  TaskStore *tasks_;
  CommentStore *comments_;
};

} // namespace taskflow::application
