cmake_minimum_required(VERSION 3.25)
project(NarrowCppTidy LANGUAGES C CXX)

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()
option(NARROW_BUILD_PLUGIN "Build the clang-tidy module" ON)
option(NARROW_BUILD_TESTING "Enable Narrow C++ regression tests" ${PROJECT_IS_TOP_LEVEL})

if(NARROW_BUILD_PLUGIN)
  find_program(NARROW_CLANG_TIDY NAMES clang-tidy REQUIRED)
  find_program(NARROW_LLVM_CONFIG NAMES llvm-config REQUIRED)
  execute_process(COMMAND ${NARROW_LLVM_CONFIG} --cmakedir
    OUTPUT_VARIABLE NARROW_LLVM_CMAKE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  find_package(LLVM REQUIRED CONFIG HINTS "${NARROW_LLVM_CMAKE_DIR}")
  find_package(Clang REQUIRED CONFIG HINTS "${LLVM_DIR}/../clang")
  execute_process(COMMAND ${NARROW_CLANG_TIDY} --version
    OUTPUT_VARIABLE NARROW_TIDY_VERSION COMMAND_ERROR_IS_FATAL ANY)
  if(NOT LLVM_VERSION_MAJOR EQUAL 23 OR NOT NARROW_TIDY_VERSION MATCHES "LLVM version 23\\.")
    message(FATAL_ERROR "This module currently targets LLVM 23 and requires a matching clang-tidy 23 executable")
  endif()
  if(NOT UNIX)
    message(FATAL_ERROR "The plugin currently supports Unix shared-library loading only")
  endif()
  message(STATUS "Building against LLVM ${LLVM_PACKAGE_VERSION}; use the matching clang-tidy")
  add_library(NarrowTidy MODULE
    checks/NarrowTidyModule.cpp
    checks/LanguageChecks.cpp
    checks/LibraryChecks.cpp
    checks/GenericChecks.cpp
    checks/PreprocessorChecks.cpp)
  # A consumer's global lint command must not try to load this module while it
  # is still being compiled. Applications opt in through narrow_enable_tidy().
  set_target_properties(NarrowTidy PROPERTIES
    CXX_CLANG_TIDY ""
    NARROW_TIDY_EXECUTABLE "${NARROW_CLANG_TIDY}")
  target_compile_features(NarrowTidy PRIVATE cxx_std_20)
  target_include_directories(NarrowTidy SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
  separate_arguments(NARROW_LLVM_DEFINITIONS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
  target_compile_options(NarrowTidy PRIVATE ${NARROW_LLVM_DEFINITIONS})
  # Resolve tidy's API from the host process; linking static tidy libraries here
  # would duplicate its registry. Use the same shared Clang/LLVM as the host.
  target_link_libraries(NarrowTidy PRIVATE clang-cpp LLVM)
  if(APPLE)
    target_link_options(NarrowTidy PRIVATE "LINKER:-undefined,dynamic_lookup")
  endif()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/NarrowTidy.cmake")

if(NARROW_BUILD_TESTING)
  find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
  enable_testing()
  if(NARROW_BUILD_PLUGIN)
    add_test(NAME custom-diagnostics
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_diagnostics.py
        --clang-tidy ${NARROW_CLANG_TIDY} --suite custom --plugin $<TARGET_FILE:NarrowTidy>)
    add_test(NAME integrated-diagnostics
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_diagnostics.py
        --clang-tidy ${NARROW_CLANG_TIDY} --suite integrated --plugin $<TARGET_FILE:NarrowTidy>)
    add_test(NAME combined-config
      COMMAND ${NARROW_CLANG_TIDY} --load=$<TARGET_FILE:NarrowTidy>
        --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy --verify-config)
    add_test(NAME module-diagnostics
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_diagnostics.py
        --clang-tidy ${NARROW_CLANG_TIDY} --suite modules --plugin $<TARGET_FILE:NarrowTidy>
        --module-compiler ${LLVM_TOOLS_BINARY_DIR}/clang++)
    set(NARROW_INVENTORY_ARGS --plugin $<TARGET_FILE:NarrowTidy> --clang-tidy ${NARROW_CLANG_TIDY})
  endif()
  add_test(NAME policy-inventory
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/inventory.py ${NARROW_INVENTORY_ARGS})
endif()
