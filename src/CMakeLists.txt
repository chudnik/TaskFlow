add_library(taskflow_domain STATIC domain/module.cpp)
add_library(TaskFlow::domain ALIAS taskflow_domain)
target_include_directories(taskflow_domain PUBLIC "${PROJECT_SOURCE_DIR}/include")

add_library(taskflow_application STATIC application/module.cpp)
add_library(TaskFlow::application ALIAS taskflow_application)
target_include_directories(taskflow_application PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_application PUBLIC TaskFlow::domain)

add_library(taskflow_infrastructure STATIC infrastructure/module.cpp)
add_library(TaskFlow::infrastructure ALIAS taskflow_infrastructure)
target_include_directories(taskflow_infrastructure PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_infrastructure PUBLIC TaskFlow::application)

add_library(taskflow_http_transport STATIC transport/http/module.cpp)
add_library(TaskFlow::http_transport ALIAS taskflow_http_transport)
target_include_directories(taskflow_http_transport PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_http_transport PUBLIC TaskFlow::application)

add_library(taskflow_websocket_transport STATIC transport/websocket/module.cpp)
add_library(TaskFlow::websocket_transport ALIAS taskflow_websocket_transport)
target_include_directories(taskflow_websocket_transport PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(taskflow_websocket_transport PUBLIC TaskFlow::application)

add_library(taskflow_service INTERFACE)
add_library(TaskFlow::service ALIAS taskflow_service)
target_link_libraries(taskflow_service INTERFACE
  TaskFlow::domain
  TaskFlow::application
  TaskFlow::infrastructure
  TaskFlow::http_transport
  TaskFlow::websocket_transport
)
