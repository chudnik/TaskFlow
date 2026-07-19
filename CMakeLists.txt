cmake_minimum_required(VERSION 3.25)

project(TaskFlow VERSION 0.1.0 LANGUAGES CXX)

option(TASKFLOW_BUILD_TESTS "Build TaskFlow tests" ON)
option(TASKFLOW_ENABLE_EXTERNAL_DEPS "Enable Conan-provided runtime dependencies" ON)
option(TASKFLOW_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(TASKFLOW_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(TASKFLOW_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(QualityTargets)

taskflow_enable_sanitizers()

add_subdirectory(src)
add_subdirectory(apps)

if(TASKFLOW_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

taskflow_add_quality_targets()
