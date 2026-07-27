#include "taskflow/application/task_query.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace taskflow::application {
namespace {

std::string trim_lower(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(),
                                      [](const unsigned char c) { return std::isspace(c) != 0; });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char c) {
                      return std::isspace(c) != 0;
                    }).base();
  value = first < last ? std::string{first, last} : std::string{};
  std::transform(value.begin(), value.end(), value.begin(),
                 [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

} // namespace

std::optional<TaskSortField> parse_task_sort_field(const std::string_view value) noexcept {
  if (value == "created_at")
    return TaskSortField::created_at;
  if (value == "updated_at")
    return TaskSortField::updated_at;
  if (value == "deadline")
    return TaskSortField::deadline;
  if (value == "priority")
    return TaskSortField::priority;
  if (value == "title")
    return TaskSortField::title;
  return std::nullopt;
}

std::optional<SortDirection> parse_sort_direction(const std::string_view value) noexcept {
  if (value == "asc")
    return SortDirection::ascending;
  if (value == "desc")
    return SortDirection::descending;
  return std::nullopt;
}

NormalizedTaskQuery normalize_task_query(TaskFilters filters, const TaskOrder order) {
  if (filters.title) {
    *filters.title = trim_lower(std::move(*filters.title));
    if (filters.title->empty()) {
      filters.title.reset();
    }
  }
  return {std::move(filters), order};
}

std::string task_query_fingerprint(const NormalizedTaskQuery &query) {
  std::ostringstream value;
  value << query.filters.project_id.to_string() << '|'
        << (query.filters.status ? domain::task_status_name(*query.filters.status) : "") << '|'
        << (query.filters.priority ? domain::task_priority_name(*query.filters.priority) : "")
        << '|' << (query.filters.assignee_id ? query.filters.assignee_id->to_string() : "") << '|'
        << (query.filters.creator_id ? query.filters.creator_id->to_string() : "") << '|'
        << (query.filters.deadline_from ? domain::format_utc(*query.filters.deadline_from) : "")
        << '|' << (query.filters.deadline_to ? domain::format_utc(*query.filters.deadline_to) : "")
        << '|' << (query.filters.overdue ? (*query.filters.overdue ? "true" : "false") : "") << '|'
        << (query.filters.title ? *query.filters.title : "") << '|'
        << static_cast<int>(query.order.field) << '|' << static_cast<int>(query.order.direction);
  return value.str();
}

} // namespace taskflow::application
