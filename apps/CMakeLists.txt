add_executable(taskflow-api api/main.cpp)
target_link_libraries(taskflow-api PRIVATE TaskFlow::service)
taskflow_set_project_warnings(taskflow-api)

add_executable(taskflow-worker worker/main.cpp)
target_link_libraries(taskflow-worker PRIVATE TaskFlow::service)
taskflow_set_project_warnings(taskflow-worker)
