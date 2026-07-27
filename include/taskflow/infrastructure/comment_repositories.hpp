#pragma once

#include "taskflow/application/comments.hpp"
#include "taskflow/infrastructure/postgres.hpp"

namespace taskflow::infrastructure {

class CommentRepository final : public application::CommentStore {
public:
  explicit CommentRepository(PostgresConnection &connection);
  [[nodiscard]] domain::Comment create_comment(const domain::Uuid &task_id,
                                               const domain::Uuid &author_id,
                                               std::string body) override;
  [[nodiscard]] std::optional<domain::Comment>
  find_comment(const domain::Uuid &comment_id) override;
  [[nodiscard]] std::vector<domain::Comment> list_comments(const domain::Uuid &task_id) override;
  [[nodiscard]] domain::Comment update_comment(const domain::Uuid &comment_id,
                                               std::string body) override;
  void delete_comment(const domain::Uuid &comment_id, const domain::Uuid &actor_id) override;

private:
  PostgresConnection *connection_;
};

} // namespace taskflow::infrastructure
