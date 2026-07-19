cmake_minimum_required(VERSION 3.25)

project(TaskFlow VERSION 0.1.0 LANGUAGES CXX)

option(TASKFLOW_BUILD_TESTS "Build TaskFlow tests" ON)
option(TASKFLOW_ENABLE_EXTERNAL_DEPS "Enable Conan-provided runtime dependencies" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
add_subdirectory(apps)

if(TASKFLOW_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
