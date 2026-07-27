#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>

namespace {

TEST(OpenApiContractTest, IsValidJsonAndCoversVersionedResources) {
  std::ifstream input{TASKFLOW_SOURCE_DIR "/openapi/taskflow-v1.json"};
  ASSERT_TRUE(input);
  const auto document = nlohmann::json::parse(input);
  EXPECT_EQ(document["openapi"], "3.1.0");
  for (const auto *path : {"/auth/register", "/projects", "/tasks", "/tasks/{taskId}/comments",
                           "/projects/{projectId}/history"})
    EXPECT_TRUE(document["paths"].contains(path)) << path;
  EXPECT_EQ(document["components"]["parameters"]["PageSize"]["schema"]["maximum"], 100);
  EXPECT_TRUE(document["components"]["securitySchemes"].contains("bearerAuth"));
}

} // namespace
