#include "taskflow/infrastructure/task_query_sql.hpp"

namespace taskflow::infrastructure {
namespace {

void append_parameter(SqlTaskQuery &query, const std::string &condition, QueryParameter value) {
  query.parameters.push_back(std::move(value));
  query.sql += " AND " + condition + " $" + std::to_string(query.parameters.size());
}

std::string sort_expression(const application::TaskSortField field) {
  switch (field) {
  case application::TaskSortField::created_at:
    return "t.created_at";
  case application::TaskSortField::updated_at:
    return "t.updated_at";
  case application::TaskSortField::deadline:
    return "t.deadline_at";
  case application::TaskSortField::priority:
    return "CASE t.priority WHEN 'low' THEN 1 WHEN 'medium' THEN 2 "
           "WHEN 'high' THEN 3 WHEN 'urgent' THEN 4 END";
  case application::TaskSortField::title:
    return "lower(t.title)";
  }
  return "t.created_at";
}

} // namespace

SqlTaskQuery build_task_list_query(const application::NormalizedTaskQuery &query,
                                   const domain::Uuid &caller_id, const domain::UtcInstant now,
                                   const std::size_t limit,
                                   const std::optional<application::TaskCursor> after) {
  SqlTaskQuery result{"SELECT t.* FROM tasks t WHERE t.deleted_at IS NULL "
                      "AND t.project_id = $1::uuid AND EXISTS (SELECT 1 FROM project_members pm "
                      "WHERE pm.project_id = t.project_id AND pm.user_id = $2::uuid)",
                      {query.filters.project_id.to_string(), caller_id.to_string()}};
  if (query.filters.status)
    append_parameter(result,
                     "t.status =", std::string{domain::task_status_name(*query.filters.status)});
  if (query.filters.priority)
    append_parameter(
        result, "t.priority =", std::string{domain::task_priority_name(*query.filters.priority)});
  if (query.filters.assignee_id)
    append_parameter(result, "t.assignee_id =", query.filters.assignee_id->to_string());
  if (query.filters.creator_id)
    append_parameter(result, "t.creator_id =", query.filters.creator_id->to_string());
  if (query.filters.deadline_from)
    append_parameter(result, "t.deadline_at >=", domain::format_utc(*query.filters.deadline_from));
  if (query.filters.deadline_to)
    append_parameter(result, "t.deadline_at <", domain::format_utc(*query.filters.deadline_to));
  if (query.filters.overdue) {
    result.parameters.push_back(domain::format_utc(now));
    const auto parameter = "$" + std::to_string(result.parameters.size()) + "::timestamptz";
    result.sql +=
        *query.filters.overdue
            ? " AND t.deadline_at < " + parameter + " AND t.status NOT IN ('done', 'cancelled')"
            : " AND (t.deadline_at IS NULL OR t.deadline_at >= " + parameter +
                  " OR t.status IN ('done', 'cancelled'))";
  }
  if (query.filters.title)
    append_parameter(result, "lower(t.title) LIKE", "%" + *query.filters.title + "%");
  const auto ascending = query.order.direction == application::SortDirection::ascending;
  const auto direction = ascending ? " ASC" : " DESC";
  if (after) {
    const auto expression = sort_expression(query.order.field);
    if (!after->sort_value) {
      result.parameters.push_back(after->task_id.to_string());
      result.sql += " AND " + expression + " IS NULL AND t.id " + (ascending ? ">" : "<") + " $" +
                    std::to_string(result.parameters.size()) + "::uuid";
    } else {
      result.parameters.push_back(*after->sort_value);
      const auto value = "$" + std::to_string(result.parameters.size());
      result.parameters.push_back(after->task_id.to_string());
      const auto id = "$" + std::to_string(result.parameters.size()) + "::uuid";
      const auto comparator = ascending ? ">" : "<";
      result.sql += " AND (((" + expression + ") " + comparator + " " + value + ") OR ((" +
                    expression + ") = " + value + " AND t.id " + comparator + " " + id + ")";
      if (query.order.field == application::TaskSortField::deadline && ascending)
        result.sql += " OR t.deadline_at IS NULL";
      result.sql += ")";
    }
  }
  result.sql += " ORDER BY " + sort_expression(query.order.field);
  result.sql += query.order.field == application::TaskSortField::deadline
                    ? (ascending ? " ASC NULLS LAST" : " DESC NULLS FIRST")
                    : direction;
  result.sql += ", t.id" + std::string{direction};
  result.parameters.push_back(std::to_string(limit));
  result.sql += " LIMIT $" + std::to_string(result.parameters.size()) + "::bigint";
  return result;
}

} // namespace taskflow::infrastructure
