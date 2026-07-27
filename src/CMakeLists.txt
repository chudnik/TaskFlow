add_library(taskflow_domain STATIC domain/module.cpp domain/common.cpp domain/identity.cpp
  domain/project.cpp domain/task.cpp domain/comment.cpp)
add_library(TaskFlow::domain ALIAS taskflow_domain)
target_include_directories(taskflow_domain PUBLIC "${PROJECT_SOURCE_DIR}/include")
taskflow_set_project_warnings(taskflow_domain)

add_library(taskflow_platform STATIC platform/runtime_config.cpp platform/runtime_lifecycle.cpp
  platform/structured_logger.cpp)
add_library(TaskFlow::platform ALIAS taskflow_platform)
target_include_directories(taskflow_platform PUBLIC "${PROJECT_SOURCE_DIR}/include")
taskflow_set_project_warnings(taskflow_platform)

add_library(taskflow_application STATIC application/module.cpp application/identity.cpp
  application/authentication_middleware.cpp application/authentication_sessions.cpp)
target_sources(taskflow_application PRIVATE application/authorization.cpp)
target_sources(taskflow_application PRIVATE application/rate_limiter.cpp)
target_sources(taskflow_application PRIVATE application/projects.cpp)
target_sources(taskflow_application PRIVATE application/tasks.cpp)
target_sources(taskflow_application PRIVATE application/task_query.cpp)
target_sources(taskflow_application PRIVATE application/task_cursor.cpp)
target_sources(taskflow_application PRIVATE application/task_list.cpp)
target_sources(taskflow_application PRIVATE application/comments.cpp)
target_sources(taskflow_application PRIVATE application/audit.cpp)
target_sources(taskflow_application PRIVATE application/notification_wakeup.cpp)
target_sources(taskflow_application PRIVATE application/job_worker.cpp)
target_sources(taskflow_application PRIVATE application/reminders.cpp)
target_sources(taskflow_application PRIVATE application/reminder_handlers.cpp)
add_library(TaskFlow::application ALIAS taskflow_application)
target_include_directories(taskflow_application PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_application PUBLIC TaskFlow::domain)
taskflow_set_project_warnings(taskflow_application)

add_library(taskflow_infrastructure STATIC
  infrastructure/module.cpp
  infrastructure/postgres.cpp
  infrastructure/schema_compatibility.cpp
)
add_library(TaskFlow::infrastructure ALIAS taskflow_infrastructure)
target_include_directories(taskflow_infrastructure PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_infrastructure PUBLIC TaskFlow::application)
taskflow_set_project_warnings(taskflow_infrastructure)

add_library(taskflow_http_contract STATIC transport/http/api_contract.cpp)
add_library(TaskFlow::http_contract ALIAS taskflow_http_contract)
target_include_directories(taskflow_http_contract PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_http_contract PUBLIC TaskFlow::application)
taskflow_set_project_warnings(taskflow_http_contract)

add_library(taskflow_http_transport STATIC transport/http/module.cpp transport/http/api_router.cpp)
add_library(TaskFlow::http_transport ALIAS taskflow_http_transport)
target_include_directories(taskflow_http_transport PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_http_transport PUBLIC TaskFlow::application TaskFlow::http_contract)
taskflow_set_project_warnings(taskflow_http_transport)

add_library(taskflow_websocket_contract STATIC transport/websocket/gateway.cpp)
add_library(TaskFlow::websocket_contract ALIAS taskflow_websocket_contract)
target_include_directories(taskflow_websocket_contract PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_websocket_contract PUBLIC TaskFlow::application)
taskflow_set_project_warnings(taskflow_websocket_contract)

add_library(taskflow_websocket_transport STATIC transport/websocket/module.cpp
  transport/websocket/runtime_controller.cpp)
add_library(TaskFlow::websocket_transport ALIAS taskflow_websocket_transport)
target_include_directories(taskflow_websocket_transport PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_websocket_transport PUBLIC TaskFlow::websocket_contract)
taskflow_set_project_warnings(taskflow_websocket_transport)

if(TASKFLOW_ENABLE_EXTERNAL_DEPS)
  find_package(Drogon CONFIG REQUIRED)
  find_package(PostgreSQL REQUIRED)
  find_package(redis++ CONFIG REQUIRED)
  find_package(spdlog CONFIG REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)
  find_package(jwt-cpp CONFIG REQUIRED)
  find_package(argon2 CONFIG REQUIRED)

  target_sources(taskflow_infrastructure PRIVATE
    infrastructure/identity_repositories.cpp
    infrastructure/authentication_sessions.cpp
    infrastructure/project_repositories.cpp
    infrastructure/task_repositories.cpp
    infrastructure/task_query_sql.cpp
    infrastructure/task_cursor.cpp
    infrastructure/comment_repositories.cpp
    infrastructure/audit_repository.cpp
    infrastructure/outbox_repository.cpp
    infrastructure/outbox_dispatcher.cpp
    infrastructure/notification_repository.cpp
    infrastructure/notification_delivery.cpp
    infrastructure/redis_notification_wakeup.cpp
    infrastructure/redis_notification_subscriber.cpp
    infrastructure/job_repository.cpp
    infrastructure/reminder_effect_repository.cpp
    infrastructure/jwt_access_token.cpp
    infrastructure/refresh_tokens.cpp
    infrastructure/redis_rate_limit.cpp
    infrastructure/password_hasher.cpp
  )
  target_sources(taskflow_http_contract PRIVATE
    transport/http/identity_controller.cpp
    transport/http/project_controller.cpp
    transport/http/membership_controller.cpp
    transport/http/task_controller.cpp
    transport/http/comment_controller.cpp
    transport/http/audit_controller.cpp
  )
  target_sources(taskflow_websocket_contract PRIVATE transport/websocket/protocol.cpp)

  target_link_libraries(taskflow_infrastructure
    PUBLIC
      libpq::libpq
      redis++::redis++_static
      spdlog::spdlog
      nlohmann_json::nlohmann_json
      jwt-cpp::jwt-cpp
      argon2::argon2
      OpenSSL::Crypto
  )
  target_compile_definitions(taskflow_infrastructure PUBLIC TASKFLOW_HAS_POSTGRES=1)
  target_compile_definitions(taskflow_infrastructure PRIVATE TASKFLOW_HAS_ARGON2=1)
  target_compile_definitions(taskflow_infrastructure PRIVATE TASKFLOW_HAS_REDIS=1)
  target_link_libraries(taskflow_platform PRIVATE spdlog::spdlog)
  target_compile_definitions(taskflow_platform PRIVATE TASKFLOW_HAS_SPDLOG=1)
  target_link_libraries(taskflow_http_transport PUBLIC Drogon::Drogon nlohmann_json::nlohmann_json)
  target_link_libraries(taskflow_http_contract PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(taskflow_http_transport PUBLIC TASKFLOW_HAS_DROGON=1)
  target_link_libraries(taskflow_websocket_transport PUBLIC Drogon::Drogon nlohmann_json::nlohmann_json)
  target_compile_definitions(taskflow_websocket_transport PUBLIC TASKFLOW_HAS_DROGON=1)
  target_link_libraries(taskflow_websocket_contract PRIVATE nlohmann_json::nlohmann_json)
endif()

if(NOT TASKFLOW_ENABLE_EXTERNAL_DEPS)
  target_compile_definitions(taskflow_platform PRIVATE TASKFLOW_HAS_SPDLOG=0)
  target_compile_definitions(taskflow_infrastructure PUBLIC TASKFLOW_HAS_POSTGRES=0)
  target_compile_definitions(taskflow_infrastructure PRIVATE TASKFLOW_HAS_ARGON2=0)
  target_compile_definitions(taskflow_infrastructure PRIVATE TASKFLOW_HAS_REDIS=0)
  target_compile_definitions(taskflow_http_transport PUBLIC TASKFLOW_HAS_DROGON=0)
  target_compile_definitions(taskflow_websocket_transport PUBLIC TASKFLOW_HAS_DROGON=0)
endif()

add_library(taskflow_runtime STATIC service/runtime.cpp)
add_library(TaskFlow::runtime ALIAS taskflow_runtime)
target_include_directories(taskflow_runtime PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_runtime PUBLIC TaskFlow::platform)
taskflow_set_project_warnings(taskflow_runtime)

add_library(taskflow_service INTERFACE)
add_library(TaskFlow::service ALIAS taskflow_service)
target_link_libraries(taskflow_service INTERFACE
  TaskFlow::runtime
  TaskFlow::platform
  TaskFlow::domain
  TaskFlow::application
  TaskFlow::infrastructure
  TaskFlow::http_transport
  TaskFlow::websocket_transport
)
