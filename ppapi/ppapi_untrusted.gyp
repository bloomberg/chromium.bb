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
          '<@(cpp_sources)',
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
        'extra_deps64': [
          '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib64/libppapi_cpp.a',
          '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib64/libppapi.a',
        ],
        'extra_deps32': [
          '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib32/libppapi_cpp.a',
          '<(PRODUCT_DIR)/obj/gen/tc_newlib/lib32/libppapi.a',
        ],
        'sources': [
          '<@(test_sources_common)',
          '<@(test_sources_nacl)',
        ],
      },
    },
  ],
}
