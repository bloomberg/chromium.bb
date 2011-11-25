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
          # Common test files
          'tests/test_case.cc',
          'tests/test_utils.cc',
          'tests/testing_instance.cc',
    
          # Compile-time tests
          'tests/test_c_includes.c',
          'tests/test_cpp_includes.cc',
          'tests/test_struct_sizes.c',
          # Test cases (PLEASE KEEP THIS SECTION IN ALPHABETICAL ORDER)
    
          # Add/uncomment PPAPI interfaces below when they get proxied.
          # Not yet proxied.
          #'test_broker.cc',
          # Not yet proxied.
          #'test_buffer.cc',
          # Not yet proxied.
          #'test_char_set.cc',
          'tests/test_cursor_control.cc',
          # Fails in DeleteDirectoryRecursively.
          # BUG: http://code.google.com/p/nativeclient/issues/detail?id=2107
          #'test_directory_reader.cc',
          'tests/test_file_io.cc',
          'tests/test_file_ref.cc',
          'tests/test_file_system.cc',
          'tests/test_memory.cc',
          'tests/test_graphics_2d.cc',
          'tests/test_image_data.cc',
          'tests/test_paint_aggregator.cc',
          # test_post_message.cc relies on synchronous scripting, which is not
          # available for untrusted tests.
          # Does not compile under nacl (uses private interface ExecuteScript).
          #'test_post_message.cc',
          'tests/test_scrollbar.cc',
          # Not yet proxied.
          #'tests/test_transport.cc',
          # Not yet proxied.
          #'tests/test_uma.cc',
          # Activating the URL loader test requires a test httpd that
          # understands HTTP POST, which our current httpd.py doesn't.
          # It also requires deactivating the tests that use FileIOTrusted
          # when running in NaCl.
          #'tests/test_url_loader.cc',
          # Does not compile under nacl (uses VarPrivate).
          #'test_url_util.cc',
          # Not yet proxied.
          #'test_video_decoder.cc',
          'tests/test_var.cc',
    
          # Deprecated test cases.
          #'tests/test_instance_deprecated.cc',
          # Var_deprecated fails in TestPassReference, and we probably won't
          # fix it.
          #'tests/test_var_deprecated.cc'
        ],
      },
    },
  ],
}
