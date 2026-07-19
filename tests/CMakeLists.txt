add_test(NAME taskflow-api-version COMMAND taskflow-api --version)
add_test(NAME taskflow-worker-version COMMAND taskflow-worker --version)

if(TASKFLOW_ENABLE_EXTERNAL_DEPS)
  find_package(GTest CONFIG REQUIRED)

  add_executable(taskflow-unit-tests unit/scaffold_test.cpp)
  target_link_libraries(taskflow-unit-tests PRIVATE TaskFlow::domain GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(taskflow-unit-tests)
endif()
