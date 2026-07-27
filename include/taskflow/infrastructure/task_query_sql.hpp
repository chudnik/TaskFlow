#pragma once

#include "taskflow/application/task_cursor.hpp"
#include "taskflow/application/task_query.hpp"
#include "taskflow/infrastructure/postgres.hpp"

namespace taskflow::infrastructure {

struct SqlTaskQuery {
  std::string sql;
  QueryParameters parameters;
};

[[nodiscard]] SqlTaskQuery
build_task_list_query(const application::NormalizedTaskQuery &query, const domain::Uuid &caller_id,
                      domain::UtcInstant now, std::size_t limit,
                      std::optional<application::TaskCursor> after = std::nullopt);

} // namespace taskflow::infrastructure
