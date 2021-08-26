# Copyright 2020 The libgav1 Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if(LIBGAV1_LIBGAV1_TESTS_CMAKE_)
  return()
endif() # LIBGAV1_LIBGAV1_TESTS_CMAKE_
set(LIBGAV1_LIBGAV1_TESTS_CMAKE_ 1)

set(libgav1_googletest "${libgav1_root}/third_party/googletest")
if(NOT LIBGAV1_ENABLE_TESTS OR NOT EXISTS "${libgav1_googletest}")
  macro(libgav1_add_tests_targets)

  endmacro()

  if(LIBGAV1_ENABLE_TESTS AND NOT EXISTS "${libgav1_googletest}")
    message(
      "GoogleTest not found, setting LIBGAV1_ENABLE_TESTS to false.\n"
      "To enable tests download the GoogleTest repository to"
      " third_party/googletest:\n\n  git \\\n    -C ${libgav1_root} \\\n"
      "    clone \\\n"
      "    https://github.com/google/googletest.git third_party/googletest\n")
    set(LIBGAV1_ENABLE_TESTS FALSE CACHE BOOL "Enables tests." FORCE)
  endif()
  return()
endif()

# Check GoogleTest compiler requirements.
if((CMAKE_CXX_COMPILER_ID
    MATCHES
    "Clang|GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5")
   OR (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19"))
  macro(libgav1_add_tests_targets)

  endmacro()

  message(
    WARNING
      "${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID} version"
      " ${CMAKE_CXX_COMPILER_VERSION}) is below the minimum requirements for"
      " GoogleTest; disabling unit tests. See"
      " https://github.com/google/googletest#compilers for more detail.")
  set(LIBGAV1_ENABLE_TESTS FALSE CACHE BOOL "Enables tests." FORCE)
  return()
endif()

list(APPEND libgav1_tests_block_utils_sources
            "${libgav1_root}/tests/block_utils.h"
            "${libgav1_root}/tests/block_utils.cc")

list(APPEND libgav1_tests_utils_sources
            "${libgav1_root}/tests/third_party/libvpx/acm_random.h"
            "${libgav1_root}/tests/third_party/libvpx/md5_helper.h"
            "${libgav1_root}/tests/third_party/libvpx/md5_utils.cc"
            "${libgav1_root}/tests/third_party/libvpx/md5_utils.h"
            "${libgav1_root}/tests/utils.h" "${libgav1_root}/tests/utils.cc")

list(APPEND libgav1_tests_utils_test_sources
            "${libgav1_root}/tests/utils_test.cc")

list(APPEND libgav1_average_blend_test_sources
            "${libgav1_source}/dsp/average_blend_test.cc")
list(APPEND libgav1_cdef_test_sources "${libgav1_source}/dsp/cdef_test.cc")
list(APPEND libgav1_convolve_test_sources
            "${libgav1_source}/dsp/convolve_test.cc")
list(APPEND libgav1_distance_weighted_blend_test_sources
            "${libgav1_source}/dsp/distance_weighted_blend_test.cc")
list(APPEND libgav1_dsp_test_sources "${libgav1_source}/dsp/dsp_test.cc")
list(APPEND libgav1_intra_edge_test_sources
            "${libgav1_source}/dsp/intra_edge_test.cc")
list(APPEND libgav1_intrapred_cfl_test_sources
            "${libgav1_source}/dsp/intrapred_cfl_test.cc")
list(APPEND libgav1_intrapred_directional_test_sources
            "${libgav1_source}/dsp/intrapred_directional_test.cc")
list(APPEND libgav1_intrapred_filter_test_sources
            "${libgav1_source}/dsp/intrapred_filter_test.cc")
list(APPEND libgav1_intrapred_test_sources
            "${libgav1_source}/dsp/intrapred_test.cc")
list(APPEND libgav1_inverse_transform_test_sources
            "${libgav1_source}/dsp/inverse_transform_test.cc")
list(APPEND libgav1_loop_filter_test_sources
            "${libgav1_source}/dsp/loop_filter_test.cc")
list(APPEND libgav1_loop_restoration_test_sources
            "${libgav1_source}/dsp/loop_restoration_test.cc")
list(APPEND libgav1_mask_blend_test_sources
            "${libgav1_source}/dsp/mask_blend_test.cc")
list(APPEND libgav1_motion_field_projection_test_sources
            "${libgav1_source}/dsp/motion_field_projection_test.cc")
list(APPEND libgav1_motion_vector_search_test_sources
            "${libgav1_source}/dsp/motion_vector_search_test.cc")
list(APPEND libgav1_super_res_test_sources
            "${libgav1_source}/dsp/super_res_test.cc")
list(APPEND libgav1_weight_mask_test_sources
            "${libgav1_source}/dsp/weight_mask_test.cc")
list(APPEND libgav1_obmc_test_sources "${libgav1_source}/dsp/obmc_test.cc")
list(APPEND libgav1_warp_test_sources "${libgav1_source}/dsp/warp_test.cc")

macro(libgav1_add_tests_targets)
  if(NOT LIBGAV1_ENABLE_TESTS)
    message(
      FATAL_ERROR
        "This version of libgav1_add_tests_targets() should only be used with"
        " LIBGAV1_ENABLE_TESTS set to true.")
  endif()
  libgav1_add_library(TEST
                      NAME
                      libgav1_gtest
                      TYPE
                      STATIC
                      SOURCES
                      "${libgav1_googletest}/googletest/src/gtest-all.cc"
                      DEFINES
                      ${libgav1_defines}
                      INCLUDES
                      ${libgav1_gtest_include_paths}
                      ${libgav1_include_paths})

  libgav1_add_library(TEST
                      NAME
                      libgav1_gtest_main
                      TYPE
                      STATIC
                      SOURCES
                      "${libgav1_googletest}/googletest/src/gtest_main.cc"
                      DEFINES
                      ${libgav1_defines}
                      INCLUDES
                      ${libgav1_gtest_include_paths}
                      ${libgav1_include_paths})

  if(ANDROID OR IOS)
    if(DEFINED LIBGAV1_THREADPOOL_USE_STD_MUTEX
       AND NOT LIBGAV1_THREADPOOL_USE_STD_MUTEX)
      set(use_absl_threading TRUE)
    endif()
  elseif(NOT
         (DEFINED
          LIBGAV1_THREADPOOL_USE_STD_MUTEX
          AND LIBGAV1_THREADPOOL_USE_STD_MUTEX))
    set(use_absl_threading TRUE)
  endif()

  if(use_absl_threading)
    list(APPEND libgav1_common_test_absl_deps absl::synchronization)
  endif()

  libgav1_add_executable(TEST
                         NAME
                         tests_utils_test
                         SOURCES
                         ${libgav1_tests_utils_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_dsp
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_library(TEST
                      NAME
                      libgav1_tests_block_utils
                      TYPE
                      OBJECT
                      SOURCES
                      ${libgav1_tests_block_utils_sources}
                      DEFINES
                      ${libgav1_defines}
                      INCLUDES
                      ${libgav1_test_include_paths})

  libgav1_add_library(TEST
                      NAME
                      libgav1_tests_utils
                      TYPE
                      OBJECT
                      SOURCES
                      ${libgav1_tests_utils_sources}
                      DEFINES
                      ${libgav1_defines}
                      INCLUDES
                      ${libgav1_test_include_paths})

  libgav1_add_executable(TEST
                         NAME
                         average_blend_test
                         SOURCES
                         ${libgav1_average_blend_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::strings
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         cdef_test
                         SOURCES
                         ${libgav1_cdef_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::strings
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         convolve_test
                         SOURCES
                         ${libgav1_convolve_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::str_format_internal
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         distance_weighted_blend_test
                         SOURCES
                         ${libgav1_distance_weighted_blend_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::strings
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         dsp_test
                         SOURCES
                         ${libgav1_dsp_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::strings
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         intrapred_cfl_test
                         SOURCES
                         ${libgav1_intrapred_cfl_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         intrapred_directional_test
                         SOURCES
                         ${libgav1_intrapred_directional_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         intrapred_filter_test
                         SOURCES
                         ${libgav1_intrapred_filter_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         intrapred_test
                         SOURCES
                         ${libgav1_intrapred_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         intra_edge_test
                         SOURCES
                         ${libgav1_intra_edge_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_tests_utils
                         libgav1_dsp
                         libgav1_utils
                         LIB_DEPS
                         absl::strings
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         inverse_transform_test
                         SOURCES
                         ${libgav1_inverse_transform_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_dsp
                         libgav1_utils
                         LIB_DEPS
                         absl::strings
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         loop_filter_test
                         SOURCES
                         ${libgav1_loop_filter_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         loop_restoration_test
                         SOURCES
                         ${libgav1_loop_restoration_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         mask_blend_test
                         SOURCES
                         ${libgav1_mask_blend_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::strings
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         motion_field_projection_test
                         SOURCES
                         ${libgav1_motion_field_projection_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::str_format_internal
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         motion_vector_search_test
                         SOURCES
                         ${libgav1_motion_vector_search_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::str_format_internal
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         obmc_test
                         SOURCES
                         ${libgav1_obmc_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::str_format_internal
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         super_res_test
                         SOURCES
                         ${libgav1_super_res_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::str_format_internal
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         warp_test
                         SOURCES
                         ${libgav1_warp_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_block_utils
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::str_format_internal
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)

  libgav1_add_executable(TEST
                         NAME
                         weight_mask_test
                         SOURCES
                         ${libgav1_weight_mask_test_sources}
                         DEFINES
                         ${libgav1_defines}
                         INCLUDES
                         ${libgav1_test_include_paths}
                         OBJLIB_DEPS
                         libgav1_decoder
                         libgav1_dsp
                         libgav1_tests_utils
                         libgav1_utils
                         LIB_DEPS
                         absl::str_format_internal
                         absl::time
                         ${libgav1_common_test_absl_deps}
                         libgav1_gtest
                         libgav1_gtest_main)
endmacro()
