function(taskflow_add_quality_targets)
  find_program(TASKFLOW_CLANG_FORMAT NAMES clang-format)
  find_program(TASKFLOW_CLANG_TIDY NAMES clang-tidy)

  file(GLOB_RECURSE TASKFLOW_FORMAT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/apps/*.cpp"
    "${PROJECT_SOURCE_DIR}/include/*.hpp"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/tests/*.cpp"
  )

  if(TASKFLOW_CLANG_FORMAT)
    add_custom_target(format
      COMMAND ${TASKFLOW_CLANG_FORMAT} -i ${TASKFLOW_FORMAT_FILES}
      COMMENT "Formatting TaskFlow C++ sources"
      VERBATIM
    )
    add_custom_target(format-check
      COMMAND ${TASKFLOW_CLANG_FORMAT} --dry-run --Werror ${TASKFLOW_FORMAT_FILES}
      COMMENT "Checking TaskFlow C++ formatting"
      VERBATIM
    )
  else()
    add_custom_target(format
      COMMAND ${CMAKE_COMMAND} -E echo "clang-format is required for target 'format'"
      COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(format-check
      COMMAND ${CMAKE_COMMAND} -E echo "clang-format is required for target 'format-check'"
      COMMAND ${CMAKE_COMMAND} -E false
    )
  endif()

  if(TASKFLOW_CLANG_TIDY)
    add_custom_target(tidy
      COMMAND ${CMAKE_COMMAND} -E env
        TASKFLOW_CLANG_TIDY=${TASKFLOW_CLANG_TIDY}
        ${CMAKE_COMMAND} -P ${PROJECT_SOURCE_DIR}/cmake/RunClangTidy.cmake
      COMMENT "Running clang-tidy"
      VERBATIM
    )
  else()
    add_custom_target(tidy
      COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy is required for target 'tidy'"
      COMMAND ${CMAKE_COMMAND} -E false
    )
  endif()
endfunction()
