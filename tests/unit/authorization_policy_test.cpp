#include "taskflow/application/authorization.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {
using namespace taskflow::application;

struct Expected {
  ProjectAction action;
  bool admin;
  bool owner;
  bool manager;
  bool member;
  bool non_member;
};

TEST(AuthorizationPolicy, EnforcesDenyByDefaultRoleMatrix) {
  constexpr std::array matrix{
      Expected{ProjectAction::read_project, true, true, true, true, false},
      Expected{ProjectAction::update_project, false, true, true, false, false},
      Expected{ProjectAction::archive_project, false, true, false, false, false},
      Expected{ProjectAction::create_task, false, true, true, true, false},
      Expected{ProjectAction::update_task, false, true, true, false, false},
      Expected{ProjectAction::delete_task, false, true, true, false, false},
      Expected{ProjectAction::manage_members, false, true, true, false, false},
      Expected{ProjectAction::manage_owner_role, false, true, false, false, false},
      Expected{ProjectAction::moderate_comment, false, true, true, false, false},
      Expected{ProjectAction::admin_moderation, true, false, false, false, false},
  };
  const PolicyService policy;
  for (const auto &row : matrix) {
    EXPECT_EQ(policy.permits({taskflow::domain::GlobalRole::admin, std::nullopt}, row.action),
              row.admin);
    EXPECT_EQ(policy.permits({taskflow::domain::GlobalRole::user, ProjectRole::owner}, row.action),
              row.owner);
    EXPECT_EQ(policy.permits({taskflow::domain::GlobalRole::user, ProjectRole::manager}, row.action),
              row.manager);
    EXPECT_EQ(policy.permits({taskflow::domain::GlobalRole::user, ProjectRole::member}, row.action),
              row.member);
    EXPECT_EQ(policy.permits({taskflow::domain::GlobalRole::user, std::nullopt}, row.action),
              row.non_member);
  }
}
} // namespace
