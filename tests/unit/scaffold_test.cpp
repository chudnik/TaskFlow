#include "taskflow/domain/module.hpp"

#include <gtest/gtest.h>

TEST(ScaffoldTest, DomainModuleIsLinked) { EXPECT_EQ(taskflow::domain::module_name(), "domain"); }
