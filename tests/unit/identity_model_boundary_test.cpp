#include "taskflow/domain/identity_models.hpp"

#include <gtest/gtest.h>

#include <type_traits>

TEST(IdentityModelBoundary, PublicModelsArePlainValuesWithoutCredentialAccessors) {
  static_assert(std::is_aggregate_v<taskflow::domain::User>);
  static_assert(std::is_aggregate_v<taskflow::domain::Session>);
  EXPECT_LT(sizeof(taskflow::domain::Session), 256U);
}
