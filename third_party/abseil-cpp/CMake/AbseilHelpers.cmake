#
# Copyright 2017 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include(CMakeParseArguments)
include(AbseilConfigureCopts)

# The IDE folder for Abseil that will be used if Abseil is included in a CMake
# project that sets
#    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# For example, Visual Studio supports folders.
set(ABSL_IDE_FOLDER Abseil)

# absl_cc_library()
#
# CMake function to imitate Bazel's cc_library rule.
#
# Parameters:
# NAME: name of target (see Note)
# HDRS: List of public header files for the library
# SRCS: List of source files for the library
# DEPS: List of other libraries to be linked in to the binary targets
# COPTS: List of private compile options
# DEFINES: List of public defines
# LINKOPTS: List of link options
# PUBLIC: Add this so that this library will be exported under absl:: (see Note).
# Also in IDE, target will appear in Abseil folder while non PUBLIC will be in Abseil/internal.
# TESTONLY: When added, this target will only be built if user passes -DABSL_RUN_TESTS=ON to CMake.
#
# Note:
# By default, absl_cc_library will always create a library named absl_internal_${NAME},
# and alias target absl::${NAME}.
# This is to reduce namespace pollution.
#
# absl_cc_library(
#   NAME
#     awesome
#   HDRS
#     "a.h"
#   SRCS
#     "a.cc"
# )
# absl_cc_library(
#   NAME
#     fantastic_lib
#   SRCS
#     "b.cc"
#   DEPS
#     absl_internal_awesome # not "awesome"!
# )
#
# If PUBLIC is set, absl_cc_library will instead create a target named
# absl_${NAME} and still an alias absl::${NAME}.
#
# absl_cc_library(
#   NAME
#     main_lib
#   ...
#   PUBLIC
# )
#
# User can then use the library as absl::main_lib (although absl_main_lib is defined too).
#
# TODO: Implement "ALWAYSLINK"
function(absl_cc_library)
  cmake_parse_arguments(ABSL_CC_LIB
    "DISABLE_INSTALL;PUBLIC;TESTONLY"
    "NAME"
    "HDRS;SRCS;COPTS;DEFINES;LINKOPTS;DEPS"
    ${ARGN}
  )

  if (NOT ABSL_CC_LIB_TESTONLY OR ABSL_RUN_TESTS)
    if (ABSL_CC_LIB_PUBLIC)
      set(_NAME "absl_${ABSL_CC_LIB_NAME}")
    else()
      set(_NAME "absl_internal_${ABSL_CC_LIB_NAME}")
    endif()

    # Check if this is a header-only library
    set(ABSL_CC_SRCS "${ABSL_CC_LIB_SRCS}")
    list(FILTER ABSL_CC_SRCS EXCLUDE REGEX ".*\\.h")
    if ("${ABSL_CC_SRCS}" STREQUAL "")
      set(ABSL_CC_LIB_IS_INTERFACE 1)
    else()
      set(ABSL_CC_LIB_IS_INTERFACE 0)
    endif()

    if(NOT ABSL_CC_LIB_IS_INTERFACE)
      add_library(${_NAME} STATIC "")
      target_sources(${_NAME} PRIVATE ${ABSL_CC_LIB_SRCS} ${ABSL_CC_LIB_HDRS})
      target_include_directories(${_NAME}
        PUBLIC ${ABSL_COMMON_INCLUDE_DIRS})
      target_compile_options(${_NAME}
        PRIVATE ${ABSL_CC_LIB_COPTS})
      target_link_libraries(${_NAME}
        PUBLIC ${ABSL_CC_LIB_DEPS}
        PRIVATE ${ABSL_CC_LIB_LINKOPTS}
      )
      target_compile_definitions(${_NAME} PUBLIC ${ABSL_CC_LIB_DEFINES})

      # Add all Abseil targets to a a folder in the IDE for organization.
      if(ABSL_CC_LIB_PUBLIC)
        set_property(TARGET ${_NAME} PROPERTY FOLDER ${ABSL_IDE_FOLDER})
      elseif(ABSL_CC_LIB_TESTONLY)
        set_property(TARGET ${_NAME} PROPERTY FOLDER ${ABSL_IDE_FOLDER}/test)
      else()
        set_property(TARGET ${_NAME} PROPERTY FOLDER ${ABSL_IDE_FOLDER}/internal)
      endif()

      # INTERFACE libraries can't have the CXX_STANDARD property set
      set_property(TARGET ${_NAME} PROPERTY CXX_STANDARD ${ABSL_CXX_STANDARD})
      set_property(TARGET ${_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
    else()
      # Generating header-only library
      add_library(${_NAME} INTERFACE)
      target_include_directories(${_NAME}
        INTERFACE ${ABSL_COMMON_INCLUDE_DIRS})
      target_link_libraries(${_NAME}
        INTERFACE ${ABSL_CC_LIB_DEPS} ${ABSL_CC_LIB_LINKOPTS}
      )
      target_compile_definitions(${_NAME} INTERFACE ${ABSL_CC_LIB_DEFINES})
    endif()

    add_library(absl::${ABSL_CC_LIB_NAME} ALIAS ${_NAME})
  endif()
endfunction()

# absl_cc_test()
#
# CMake function to imitate Bazel's cc_test rule.
#
# Parameters:
# NAME: name of target (see Usage below)
# SRCS: List of source files for the binary
# DEPS: List of other libraries to be linked in to the binary targets
# COPTS: List of private compile options
# DEFINES: List of public defines
# LINKOPTS: List of link options
#
# Note:
# By default, absl_cc_test will always create a binary named absl_${NAME}.
# This will also add it to ctest list as absl_${NAME}.
#
# Usage:
# absl_cc_library(
#   NAME
#     awesome
#   HDRS
#     "a.h"
#   SRCS
#     "a.cc"
#   PUBLIC
# )
#
# absl_cc_test(
#   NAME
#     awesome_test
#   SRCS
#     "awesome_test.cc"
#   DEPS
#     absl::awesome
#     gmock
#     gtest_main
# )
function(absl_cc_test)
  if(NOT ABSL_RUN_TESTS)
    return()
  endif()

  cmake_parse_arguments(ABSL_CC_TEST
    ""
    "NAME"
    "SRCS;COPTS;DEFINES;LINKOPTS;DEPS"
    ${ARGN}
  )

  set(_NAME "absl_${ABSL_CC_TEST_NAME}")
  add_executable(${_NAME} "")
  target_sources(${_NAME} PRIVATE ${ABSL_CC_TEST_SRCS})
  target_include_directories(${_NAME}
    PUBLIC ${ABSL_COMMON_INCLUDE_DIRS}
    PRIVATE ${GMOCK_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS}
  )
  target_compile_definitions(${_NAME}
    PUBLIC ${ABSL_CC_TEST_DEFINES}
  )
  target_compile_options(${_NAME}
    PRIVATE ${ABSL_CC_TEST_COPTS}
  )
  target_link_libraries(${_NAME}
    PUBLIC ${ABSL_CC_TEST_DEPS}
    PRIVATE ${ABSL_CC_TEST_LINKOPTS}
  )
  # Add all Abseil targets to a a folder in the IDE for organization.
  set_property(TARGET ${_NAME} PROPERTY FOLDER ${ABSL_IDE_FOLDER}/test)

  set_property(TARGET ${_NAME} PROPERTY CXX_STANDARD ${ABSL_CXX_STANDARD})
  set_property(TARGET ${_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

  add_test(NAME ${_NAME} COMMAND ${_NAME})
endfunction()


function(check_target my_target)
  if(NOT TARGET ${my_target})
    message(FATAL_ERROR " ABSL: compiling absl requires a ${my_target} CMake target in your project,
                   see CMake/README.md for more details")
  endif(NOT TARGET ${my_target})
endfunction()
