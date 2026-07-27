#include "taskflow/infrastructure/comment_repositories.hpp"

namespace taskflow::infrastructure {
namespace {

constexpr std::string_view columns =
    "id::text, task_id::text, author_id::text, body, "
    "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "CASE WHEN deleted_at IS NULL THEN NULL ELSE "
    "to_char(deleted_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') END, "
    "deleted_by::text";

const std::string &required(const QueryResult &result, const std::size_t row,
                            const std::size_t column) {
  const auto &value = result.value(row, column);
  if (!value)
    throw RepositoryError{RepositoryErrorCode::unexpected, "comment row contains null"};
  return *value;
}

domain::Uuid uuid(const std::string &value) {
  const auto parsed = domain::Uuid::parse(value);
  if (!parsed)
    throw RepositoryError{RepositoryErrorCode::unexpected, "comment UUID is invalid"};
  return *parsed;
}

domain::UtcInstant instant(const std::string &value) {
  const auto parsed = domain::parse_utc(value);
  if (!parsed)
    throw RepositoryError{RepositoryErrorCode::unexpected, "comment timestamp is invalid"};
  return *parsed;
}

domain::Comment comment_from(const QueryResult &result, const std::size_t row = 0) {
  return {uuid(required(result, row, 0)),
          uuid(required(result, row, 1)),
          uuid(required(result, row, 2)),
          required(result, row, 3),
          instant(required(result, row, 4)),
          instant(required(result, row, 5)),
          result.value(row, 6) ? std::optional{instant(*result.value(row, 6))} : std::nullopt,
          result.value(row, 7) ? std::optional{uuid(*result.value(row, 7))} : std::nullopt};
}

} // namespace

CommentRepository::CommentRepository(PostgresConnection &connection) : connection_{&connection} {}

domain::Comment CommentRepository::create_comment(const domain::Uuid &task_id,
                                                  const domain::Uuid &author_id, std::string body) {
  const auto result =
      connection_->execute("INSERT INTO comments(id, task_id, author_id, body) "
                           "SELECT $1::uuid, t.id, $3::uuid, $4 FROM tasks t "
                           "WHERE t.id = $2::uuid AND t.deleted_at IS NULL RETURNING " +
                               std::string{columns},
                           {domain::Uuid::generate().to_string(), task_id.to_string(),
                            author_id.to_string(), std::move(body)});
  if (result.row_count() == 0)
    throw RepositoryError{RepositoryErrorCode::not_found, "active task not found"};
  return comment_from(result);
}

std::optional<domain::Comment> CommentRepository::find_comment(const domain::Uuid &comment_id) {
  const auto result =
      connection_->execute("SELECT " + std::string{columns} +
                               " FROM comments WHERE id = $1::uuid AND deleted_at IS NULL",
                           {comment_id.to_string()});
  return result.row_count() == 0 ? std::nullopt : std::optional{comment_from(result)};
}

std::vector<domain::Comment> CommentRepository::list_comments(const domain::Uuid &task_id) {
  const auto result =
      connection_->execute("SELECT " + std::string{columns} +
                               " FROM comments WHERE task_id = $1::uuid AND deleted_at IS NULL "
                               "ORDER BY created_at, id",
                           {task_id.to_string()});
  std::vector<domain::Comment> comments;
  comments.reserve(result.row_count());
  for (std::size_t row = 0; row < result.row_count(); ++row)
    comments.push_back(comment_from(result, row));
  return comments;
}

domain::Comment CommentRepository::update_comment(const domain::Uuid &comment_id,
                                                  std::string body) {
  const auto result =
      connection_->execute("UPDATE comments SET body = $2, updated_at = clock_timestamp() "
                           "WHERE id = $1::uuid AND deleted_at IS NULL RETURNING " +
                               std::string{columns},
                           {comment_id.to_string(), std::move(body)});
  if (result.row_count() == 0)
    throw RepositoryError{RepositoryErrorCode::not_found, "comment not found"};
  return comment_from(result);
}

void CommentRepository::delete_comment(const domain::Uuid &comment_id,
                                       const domain::Uuid &actor_id) {
  const auto result = connection_->execute(
      "UPDATE comments SET deleted_at = clock_timestamp(), deleted_by = $2::uuid, "
      "updated_at = clock_timestamp() WHERE id = $1::uuid AND deleted_at IS NULL",
      {comment_id.to_string(), actor_id.to_string()});
  if (result.affected_rows() == 0)
    throw RepositoryError{RepositoryErrorCode::not_found, "comment not found"};
}

} // namespace taskflow::infrastructure
