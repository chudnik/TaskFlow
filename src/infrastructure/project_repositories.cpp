#include "taskflow/infrastructure/project_repositories.hpp"

#include <utility>

namespace taskflow::infrastructure {
namespace {

[[nodiscard]] const std::string &required(const QueryResult &result, const std::size_t row,
                                          const std::size_t column) {
  const auto &value = result.value(row, column);
  if (!value) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "project row contains null"};
  }
  return *value;
}

[[nodiscard]] domain::Uuid uuid(const std::string &value) {
  auto parsed = domain::Uuid::parse(value);
  if (!parsed) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "project row contains invalid UUID"};
  }
  return *parsed;
}

[[nodiscard]] domain::UtcInstant instant(const std::string &value) {
  auto parsed = domain::parse_utc(value);
  if (!parsed) {
    throw RepositoryError{RepositoryErrorCode::unexpected,
                          "project row contains invalid timestamp"};
  }
  return *parsed;
}

[[nodiscard]] domain::Project project_from(const QueryResult &result, const std::size_t row = 0) {
  return domain::Project{
      uuid(required(result, row, 0)),
      required(result, row, 1),
      required(result, row, 2),
      uuid(required(result, row, 3)),
      instant(required(result, row, 4)),
      instant(required(result, row, 5)),
      result.value(row, 6) ? std::optional{instant(*result.value(row, 6))} : std::nullopt,
      result.value(row, 7) ? std::optional{uuid(*result.value(row, 7))} : std::nullopt};
}

[[nodiscard]] domain::ProjectMembership membership_from(const QueryResult &result,
                                                        const std::size_t row = 0) {
  const auto role = domain::parse_project_role(required(result, row, 2));
  if (!role) {
    throw RepositoryError{RepositoryErrorCode::unexpected, "membership row contains invalid role"};
  }
  return domain::ProjectMembership{uuid(required(result, row, 0)), uuid(required(result, row, 1)),
                                   *role, instant(required(result, row, 3)),
                                   instant(required(result, row, 4))};
}

constexpr std::string_view project_columns =
    "id::text, name, description, created_by::text, "
    "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "CASE WHEN archived_at IS NULL THEN NULL ELSE "
    "to_char(archived_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') END, "
    "archived_by::text";
constexpr std::string_view membership_columns =
    "project_id::text, user_id::text, role, "
    "to_char(joined_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"'), "
    "to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"')";

void lock_project(Transaction &transaction, const domain::Uuid &project_id) {
  const auto locked = transaction.execute("SELECT id FROM projects WHERE id = $1::uuid FOR UPDATE",
                                          {project_id.to_string()});
  if (locked.row_count() == 0) {
    throw RepositoryError{RepositoryErrorCode::not_found, "project not found"};
  }
}

void protect_final_owner(Transaction &transaction, const domain::Uuid &project_id,
                         const domain::Uuid &user_id) {
  const auto current = transaction.execute(
      "SELECT role FROM project_members WHERE project_id = $1::uuid AND user_id = $2::uuid",
      {project_id.to_string(), user_id.to_string()});
  if (current.row_count() == 0) {
    throw RepositoryError{RepositoryErrorCode::not_found, "project membership not found"};
  }
  if (required(current, 0, 0) != "owner") {
    return;
  }
  const auto owners = transaction.execute(
      "SELECT count(*) FROM project_members WHERE project_id = $1::uuid AND role = 'owner'",
      {project_id.to_string()});
  if (required(owners, 0, 0) == "1") {
    throw RepositoryError{RepositoryErrorCode::conflict, "project must retain at least one owner"};
  }
}

} // namespace

ProjectRepository::ProjectRepository(PostgresConnection &connection) : connection_{&connection} {}

domain::Project ProjectRepository::create_with_owner(std::string name, std::string description,
                                                     const domain::Uuid &owner_id) {
  const auto id = domain::Uuid::generate();
  auto transaction = connection_->transaction();
  const auto result = transaction.execute(
      "INSERT INTO projects(id, name, description, created_by) "
      "VALUES($1::uuid, $2, $3, $4::uuid) RETURNING " +
          std::string{project_columns},
      {id.to_string(), std::move(name), std::move(description), owner_id.to_string()});
  static_cast<void>(transaction.execute(
      "INSERT INTO project_members(project_id, user_id, role) VALUES($1::uuid, $2::uuid, 'owner')",
      {id.to_string(), owner_id.to_string()}));
  transaction.commit();
  return project_from(result);
}

domain::Project ProjectRepository::create_project(std::string name, std::string description,
                                                  const domain::Uuid &owner_id) {
  return create_with_owner(std::move(name), std::move(description), owner_id);
}

std::optional<domain::Project> ProjectRepository::find_by_id(const domain::Uuid &project_id) {
  const auto result = connection_->execute("SELECT " + std::string{project_columns} +
                                               " FROM projects WHERE id = $1::uuid",
                                           {project_id.to_string()});
  return result.row_count() == 0 ? std::nullopt
                                 : std::optional<domain::Project>{project_from(result)};
}

std::optional<domain::Project> ProjectRepository::find_project(const domain::Uuid &project_id) {
  return find_by_id(project_id);
}

std::optional<domain::Project>
ProjectRepository::find_visible_project(const domain::Uuid &project_id, const domain::Uuid &user_id,
                                        const bool include_all) {
  const auto result =
      include_all
          ? connection_->execute("SELECT " + std::string{project_columns} +
                                     " FROM projects WHERE id = $1::uuid",
                                 {project_id.to_string()})
          : connection_->execute("SELECT " + std::string{project_columns} +
                                     " FROM projects p WHERE p.id = $1::uuid AND EXISTS "
                                     "(SELECT 1 FROM project_members pm WHERE pm.project_id = p.id "
                                     "AND pm.user_id = $2::uuid)",
                                 {project_id.to_string(), user_id.to_string()});
  return result.row_count() == 0 ? std::nullopt
                                 : std::optional<domain::Project>{project_from(result)};
}

std::optional<domain::ProjectRole> ProjectRepository::find_role(const domain::Uuid &project_id,
                                                                const domain::Uuid &user_id) {
  const auto result = connection_->execute(
      "SELECT role FROM project_members WHERE project_id = $1::uuid AND user_id = $2::uuid",
      {project_id.to_string(), user_id.to_string()});
  return result.row_count() == 0 ? std::nullopt
                                 : domain::parse_project_role(required(result, 0, 0));
}

domain::Project ProjectRepository::update_project(const domain::Uuid &project_id, std::string name,
                                                  std::string description) {
  const auto result = connection_->execute(
      "UPDATE projects SET name = $2, description = $3, updated_at = clock_timestamp() "
      "WHERE id = $1::uuid AND archived_at IS NULL RETURNING " +
          std::string{project_columns},
      {project_id.to_string(), std::move(name), std::move(description)});
  if (result.row_count() == 0) {
    throw RepositoryError{RepositoryErrorCode::not_found, "active project not found"};
  }
  return project_from(result);
}

domain::Project ProjectRepository::archive_project(const domain::Uuid &project_id,
                                                   const domain::Uuid &actor_id) {
  const auto result = connection_->execute(
      "UPDATE projects SET archived_at = clock_timestamp(), archived_by = $2::uuid, "
      "updated_at = clock_timestamp() WHERE id = $1::uuid AND archived_at IS NULL RETURNING " +
          std::string{project_columns},
      {project_id.to_string(), actor_id.to_string()});
  if (result.row_count() == 0) {
    throw RepositoryError{RepositoryErrorCode::not_found, "active project not found"};
  }
  return project_from(result);
}

std::vector<domain::Project> ProjectRepository::list_projects(const domain::Uuid &user_id,
                                                              const bool include_all) {
  const auto result =
      include_all
          ? connection_->execute("SELECT " + std::string{project_columns} +
                                 " FROM projects ORDER BY updated_at DESC, id")
          : connection_->execute("SELECT " + std::string{project_columns} +
                                     " FROM projects p WHERE EXISTS (SELECT 1 FROM project_members "
                                     "pm WHERE pm.project_id = p.id AND pm.user_id = $1::uuid) "
                                     "ORDER BY updated_at DESC, id",
                                 {user_id.to_string()});
  std::vector<domain::Project> projects;
  projects.reserve(result.row_count());
  for (std::size_t row = 0; row < result.row_count(); ++row) {
    projects.push_back(project_from(result, row));
  }
  return projects;
}

ProjectMembershipRepository::ProjectMembershipRepository(PostgresConnection &connection)
    : connection_{&connection} {}

domain::ProjectMembership ProjectMembershipRepository::add(const domain::Uuid &project_id,
                                                           const domain::Uuid &user_id,
                                                           const domain::ProjectRole role) {
  auto transaction = connection_->transaction();
  lock_project(transaction, project_id);
  const auto result = transaction.execute(
      "INSERT INTO project_members(project_id, user_id, role) VALUES($1::uuid, $2::uuid, $3) "
      "RETURNING " +
          std::string{membership_columns},
      {project_id.to_string(), user_id.to_string(), std::string{domain::project_role_name(role)}});
  transaction.commit();
  return membership_from(result);
}

domain::ProjectMembership ProjectMembershipRepository::add_membership(
    const domain::Uuid &project_id, const domain::Uuid &user_id, const domain::ProjectRole role) {
  return add(project_id, user_id, role);
}

std::optional<domain::ProjectMembership>
ProjectMembershipRepository::find(const domain::Uuid &project_id, const domain::Uuid &user_id) {
  const auto result = connection_->execute(
      "SELECT " + std::string{membership_columns} +
          " FROM project_members WHERE project_id = $1::uuid AND user_id = $2::uuid",
      {project_id.to_string(), user_id.to_string()});
  return result.row_count() == 0
             ? std::nullopt
             : std::optional<domain::ProjectMembership>{membership_from(result)};
}

std::optional<domain::ProjectMembership>
ProjectMembershipRepository::find_membership(const domain::Uuid &project_id,
                                             const domain::Uuid &user_id) {
  return find(project_id, user_id);
}

std::vector<domain::ProjectMembership>
ProjectMembershipRepository::list(const domain::Uuid &project_id) {
  const auto result = connection_->execute(
      "SELECT " + std::string{membership_columns} +
          " FROM project_members WHERE project_id = $1::uuid ORDER BY joined_at, user_id",
      {project_id.to_string()});
  std::vector<domain::ProjectMembership> memberships;
  memberships.reserve(result.row_count());
  for (std::size_t row = 0; row < result.row_count(); ++row) {
    memberships.push_back(membership_from(result, row));
  }
  return memberships;
}

std::vector<domain::ProjectMembership>
ProjectMembershipRepository::list_memberships(const domain::Uuid &project_id) {
  return list(project_id);
}

domain::ProjectMembership ProjectMembershipRepository::change_role(const domain::Uuid &project_id,
                                                                   const domain::Uuid &user_id,
                                                                   const domain::ProjectRole role) {
  auto transaction = connection_->transaction();
  lock_project(transaction, project_id);
  if (role != domain::ProjectRole::owner) {
    protect_final_owner(transaction, project_id, user_id);
  }
  const auto result = transaction.execute(
      "UPDATE project_members SET role = $3, updated_at = clock_timestamp() "
      "WHERE project_id = $1::uuid AND user_id = $2::uuid RETURNING " +
          std::string{membership_columns},
      {project_id.to_string(), user_id.to_string(), std::string{domain::project_role_name(role)}});
  if (result.row_count() == 0) {
    throw RepositoryError{RepositoryErrorCode::not_found, "project membership not found"};
  }
  transaction.commit();
  return membership_from(result);
}

domain::ProjectMembership ProjectMembershipRepository::change_membership_role(
    const domain::Uuid &project_id, const domain::Uuid &user_id, const domain::ProjectRole role) {
  return change_role(project_id, user_id, role);
}

void ProjectMembershipRepository::remove(const domain::Uuid &project_id,
                                         const domain::Uuid &user_id) {
  auto transaction = connection_->transaction();
  lock_project(transaction, project_id);
  protect_final_owner(transaction, project_id, user_id);
  static_cast<void>(transaction.execute(
      "UPDATE tasks SET assignee_id = NULL, version = version + 1, "
      "updated_at = clock_timestamp() "
      "WHERE project_id = $1::uuid AND assignee_id = $2::uuid AND deleted_at IS NULL",
      {project_id.to_string(), user_id.to_string()}));
  const auto result = transaction.execute(
      "DELETE FROM project_members WHERE project_id = $1::uuid AND user_id = $2::uuid",
      {project_id.to_string(), user_id.to_string()});
  if (result.affected_rows() == 0) {
    throw RepositoryError{RepositoryErrorCode::not_found, "project membership not found"};
  }
  transaction.commit();
}

void ProjectMembershipRepository::remove_membership(const domain::Uuid &project_id,
                                                    const domain::Uuid &user_id) {
  remove(project_id, user_id);
}

} // namespace taskflow::infrastructure
