# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This GYP file defines untrusted (NaCl) targets.  All targets in this
# file should be conditionally depended upon via 'disable_nacl!=1' to avoid
# requiring NaCl sources for building.

{
  'includes': [
    '../native_client/build/untrusted.gypi',
    'ppapi_sources.gypi',
  ],
  'targets': [
    {
      'target_name': 'ppapi_cpp_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libppapi_cpp.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'sources': [
          '<@(cpp_source_files)',
          'cpp/module_embedder.h',
          'cpp/ppp_entrypoints.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'ppapi_nacl_tests',
      'type': 'none',
      'dependencies': [
         'ppapi_cpp_lib',
         'native_client/native_client.gyp:ppapi_lib',
         'native_client/native_client.gyp:nacl_irt',
       ],
      'variables': {
        'nexe_target': 'ppapi_nacl_tests',
        'build_glibc': 0,
        'build_newlib': 1,
        'include_dirs': [
          'lib/gl/include',
          '..',
        ],
        'link_flags': [
          '-lppapi_cpp',
          '-lppapi',
        ],
        # TODO(bradchen): get rid of extra_deps64 and extra_deps32
        # once native_client/build/untrusted.gypi no longer needs them.
        'extra_deps64': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi.a',
        ],
        'extra_deps32': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi.a',
        ],
        'extra_deps_newlib64': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi.a',
        ],
        'extra_deps_newlib32': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi.a',
        ],
        'extra_deps_glibc64': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi.a',
        ],
        'extra_deps_glibc32': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi.a',
        ],
        'extra_deps_arm': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libppapi.a',
        ],
        'sources': [
          '<@(test_common_source_files)',
          '<@(test_nacl_source_files)',
        ],
      },
    },
  ],
}
