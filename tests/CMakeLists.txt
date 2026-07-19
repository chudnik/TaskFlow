add_test(NAME taskflow-api-version COMMAND taskflow-api --version)
add_test(NAME taskflow-worker-version COMMAND taskflow-worker --version)

if(TASKFLOW_ENABLE_EXTERNAL_DEPS)
  find_package(GTest CONFIG REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)

  add_executable(taskflow-unit-tests
    unit/scaffold_test.cpp
    unit/runtime_config_test.cpp
    unit/structured_logger_test.cpp
    unit/schema_compatibility_test.cpp
  )
  target_link_libraries(taskflow-unit-tests PRIVATE
    TaskFlow::domain
    TaskFlow::platform
    TaskFlow::infrastructure
    GTest::gtest_main
    nlohmann_json::nlohmann_json
  )
  taskflow_set_project_warnings(taskflow-unit-tests)

  include(GoogleTest)
  gtest_discover_tests(taskflow-unit-tests)
endif()

foreach(TASKFLOW_EXECUTABLE IN ITEMS taskflow-api taskflow-worker)
  add_test(
    NAME ${TASKFLOW_EXECUTABLE}-missing-signing-secret
    COMMAND ${CMAKE_COMMAND}
      -DTASKFLOW_EXECUTABLE=$<TARGET_FILE:${TASKFLOW_EXECUTABLE}>
      -P ${PROJECT_SOURCE_DIR}/cmake/ExpectStartupFailure.cmake
  )
endforeach()
