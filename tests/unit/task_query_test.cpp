#include "taskflow/application/task_query.hpp"
#include "taskflow/infrastructure/task_query_sql.hpp"

#include <gtest/gtest.h>

namespace {
using namespace taskflow;

TEST(TaskQueryTest, NormalizesAndParameterizesEveryFilter) {
  const auto project = domain::Uuid::generate();
  const auto assignee = domain::Uuid::generate();
  const auto creator = domain::Uuid::generate();
  application::TaskFilters filters{project,
                                   domain::TaskStatus::in_progress,
                                   domain::TaskPriority::urgent,
                                   assignee,
                                   creator,
                                   domain::parse_utc("2026-07-01T00:00:00Z"),
                                   domain::parse_utc("2026-08-01T00:00:00Z"),
                                   true,
                                   "  RELEASE  "};
  const auto normalized = application::normalize_task_query(
      std::move(filters),
      {application::TaskSortField::deadline, application::SortDirection::ascending});
  EXPECT_EQ(normalized.filters.title, "release");
  const auto sql = infrastructure::build_task_list_query(
      normalized, domain::Uuid::generate(), *domain::parse_utc("2026-07-27T10:00:00Z"), 26);
  EXPECT_NE(sql.sql.find("EXISTS (SELECT 1 FROM project_members"), std::string::npos);
  EXPECT_NE(sql.sql.find("t.status = $3"), std::string::npos);
  EXPECT_NE(sql.sql.find("lower(t.title) LIKE $10"), std::string::npos);
  EXPECT_NE(sql.sql.find("ORDER BY t.deadline_at ASC NULLS LAST, t.id ASC"), std::string::npos);
  EXPECT_EQ(sql.parameters.size(), 11U);
  EXPECT_EQ(*sql.parameters[9], "%release%");
  EXPECT_EQ(*sql.parameters[10], "26");
  EXPECT_EQ(std::count(sql.sql.begin(), sql.sql.end(), '$'), 11);
}

TEST(TaskQueryTest, ProducesStableFingerprintAndSortParsing) {
  const auto project = domain::Uuid::generate();
  auto first = application::normalize_task_query(
      {project, {}, {}, {}, {}, {}, {}, false, " Foo "},
      {application::TaskSortField::title, application::SortDirection::descending});
  auto second = application::normalize_task_query(
      {project, {}, {}, {}, {}, {}, {}, false, "foo"},
      {application::TaskSortField::title, application::SortDirection::descending});
  EXPECT_EQ(application::task_query_fingerprint(first),
            application::task_query_fingerprint(second));
  EXPECT_EQ(application::parse_task_sort_field("priority"), application::TaskSortField::priority);
  EXPECT_EQ(application::parse_sort_direction("desc"), application::SortDirection::descending);
  EXPECT_FALSE(application::parse_task_sort_field("random"));
}

} // namespace
