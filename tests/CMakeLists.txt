add_test(NAME taskflow-api-version COMMAND taskflow-api --version)
add_test(NAME taskflow-worker-version COMMAND taskflow-worker --version)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME documentation.russian
  COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/check_russian_docs.py
    --root ${PROJECT_SOURCE_DIR}
)
add_test(
  NAME documentation.russian-checker-tests
  COMMAND ${Python3_EXECUTABLE} -m unittest
    tests/documentation/test_check_russian_docs.py
)
set_tests_properties(documentation.russian-checker-tests PROPERTIES
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

if(TASKFLOW_ENABLE_EXTERNAL_DEPS)
  find_package(GTest CONFIG REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)

  add_executable(taskflow-unit-tests
    unit/scaffold_test.cpp
    unit/runtime_config_test.cpp
    unit/runtime_lifecycle_test.cpp
    unit/runtime_owner_test.cpp
    unit/structured_logger_test.cpp
    unit/schema_compatibility_test.cpp
    unit/common_primitives_test.cpp
    unit/api_router_test.cpp
    unit/identity_security_test.cpp
    unit/identity_model_boundary_test.cpp
    unit/identity_use_cases_test.cpp
    unit/jwt_access_token_test.cpp
    unit/refresh_tokens_test.cpp
    unit/authentication_middleware_test.cpp
    unit/authentication_sessions_test.cpp
    unit/authorization_policy_test.cpp
    unit/rate_limiter_test.cpp
    unit/project_model_test.cpp
    unit/project_use_cases_test.cpp
    unit/membership_use_cases_test.cpp
    unit/task_model_test.cpp
    unit/task_use_cases_test.cpp
    unit/task_query_test.cpp
    unit/task_cursor_test.cpp
    unit/audit_test.cpp
    unit/websocket_gateway_test.cpp
    unit/notification_wakeup_test.cpp
    unit/websocket_protocol_test.cpp
    unit/job_worker_test.cpp
    unit/reminder_scheduler_test.cpp
    unit/reminder_handler_test.cpp
    unit/openapi_contract_test.cpp
  )
  target_compile_definitions(taskflow-unit-tests PRIVATE
    TASKFLOW_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  target_link_libraries(taskflow-unit-tests PRIVATE
    TaskFlow::runtime
    TaskFlow::domain
    TaskFlow::platform
    TaskFlow::infrastructure
    TaskFlow::http_contract
    TaskFlow::websocket_contract
    GTest::gtest_main
    nlohmann_json::nlohmann_json
  )
  taskflow_set_project_warnings(taskflow-unit-tests)

  include(GoogleTest)
  gtest_discover_tests(taskflow-unit-tests)

  add_executable(taskflow-postgres-integration-tests
    integration/postgres_foundation_test.cpp
    integration/refresh_token_rotation_test.cpp
    integration/project_repositories_test.cpp
    integration/docker_compose_e2e_test.cpp
  )
  target_link_libraries(taskflow-postgres-integration-tests PRIVATE
    TaskFlow::infrastructure
    GTest::gtest_main
  )
  taskflow_set_project_warnings(taskflow-postgres-integration-tests)
  gtest_discover_tests(taskflow-postgres-integration-tests
    TEST_PREFIX "integration."
  )

  add_test(
    NAME integration.health-endpoints
    COMMAND sh ${PROJECT_SOURCE_DIR}/tests/integration/health_endpoints_test.sh
      $<TARGET_FILE:taskflow-api>
      ${PROJECT_SOURCE_DIR}/tests/integration
  )
  set_tests_properties(integration.health-endpoints PROPERTIES
    SKIP_RETURN_CODE 77
    RUN_SERIAL TRUE
    TIMEOUT 30
  )
endif()

foreach(TASKFLOW_EXECUTABLE IN ITEMS taskflow-api taskflow-worker)
  add_test(
    NAME ${TASKFLOW_EXECUTABLE}-missing-signing-secret
    COMMAND ${CMAKE_COMMAND}
      -DTASKFLOW_EXECUTABLE=$<TARGET_FILE:${TASKFLOW_EXECUTABLE}>
      -P ${PROJECT_SOURCE_DIR}/cmake/ExpectStartupFailure.cmake
  )
endforeach()
