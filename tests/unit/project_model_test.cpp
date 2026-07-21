#include "taskflow/domain/project.hpp"

#include <gtest/gtest.h>

namespace taskflow::domain {
namespace {

TEST(ProjectModelTest, ValidatesFieldsAndRoles) {
  EXPECT_TRUE(validate_project_fields("TaskFlow", "Backend project").empty());
  EXPECT_FALSE(validate_project_fields("   ", "").empty());
  EXPECT_FALSE(validate_project_fields("TaskFlow", std::string(10'001, 'x')).empty());

  EXPECT_EQ(parse_project_role("owner"), ProjectRole::owner);
  EXPECT_EQ(parse_project_role("manager"), ProjectRole::manager);
  EXPECT_EQ(parse_project_role("member"), ProjectRole::member);
  EXPECT_EQ(parse_project_role("administrator"), std::nullopt);
  EXPECT_EQ(project_role_name(ProjectRole::owner), "owner");
}

} // namespace
} // namespace taskflow::domain
