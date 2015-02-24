# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../ppapi/ppapi_nacl_test_common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ppapi_tests_extensions_background_keepalive',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_tests_extensions_background_keepalive',
        # Only newlib build is used in tests, no need to build others.
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'test_data/ppapi/tests/extensions/background_keepalive',
        'sources': [
          'background_keepalive/background.cc',
        ],
        'test_files': [
          'background_keepalive/background.js',
          'background_keepalive/manifest.json',
        ],
      },
    },
    {
      'target_name': 'ppapi_tests_extensions_media_galleries',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_tests_extensions_media_galleries',
        # Only newlib build is used in tests, no need to build others.
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'test_data/ppapi/tests/extensions/media_galleries',
        'sources': [
          '<(DEPTH)/ppapi/tests/test_utils.cc',
          '<(DEPTH)/ppapi/tests/test_utils.h',
          'media_galleries/test_galleries.cc',
        ],
        'test_files': [
          'media_galleries/background.js',
          'media_galleries/index.html',
          'media_galleries/manifest.json',
          'media_galleries/test.js',
        ],
      },
    },
    {
      'target_name': 'ppapi_tests_extensions_packaged_app',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_tests_extensions_packaged_app',
        'build_newlib': 1,
        'build_glibc': 0,
        'nexe_destination_dir': 'test_data/ppapi/tests/extensions/packaged_app',
        'sources': [
          'packaged_app/test_packaged_app.cc'
        ],
        'test_files': [
          'packaged_app/controller.js',
          'packaged_app/index.html',
          'packaged_app/main.js',
          'packaged_app/manifest.json',
          'packaged_app/test_file.txt',
          'packaged_app/test_file2.txt',
        ],
        'create_nmf_args_portable': [
          # Add lots of "files" entries to make sure that open_resource can
          # handle more files than FileDescriptorSet::kMaxDescriptorsPerMessage.
          '-xtest_file0:test_file.txt',
          '-xtest_file1:test_file2.txt',
          '-xtest_file2:test_file.txt',
          '-xtest_file3:test_file2.txt',
          '-xtest_file4:test_file.txt',
          '-xtest_file5:test_file2.txt',
          '-xtest_file6:test_file.txt',
          '-xtest_file7:test_file2.txt',
        ],
      },
      'conditions': [
        ['(target_arch=="ia32" or target_arch=="x64") and OS=="linux"', {
          # Enable nonsfi testing only on ia32-linux environment.
          # See chrome/test/data/nacl/nacl_test_data.gyp for more info.
          'variables': {
            'build_pnacl_newlib': 1,
            'translate_pexe_with_build': 1,
            'enable_x86_32_nonsfi': 1,
          },
        }],
      ],
      # Shim is a dependency for the nexe because we pre-translate.
      'dependencies': [
        '<(DEPTH)/ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:aot',
      ],
    },
    {
      'target_name': 'ppapi_tests_extensions_socket_permissions',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_tests_extensions_socket_permissions',
        # Only newlib build is used in tests, no need to build others.
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'test_data/ppapi/tests/extensions/socket_permissions',
        'sources': [
          'socket_permissions/test_socket_permissions.cc',
        ],
        'test_files': [
          'socket_permissions/controller.js',
          'socket_permissions/index.html',
          'socket_permissions/main.js',
          'socket_permissions/manifest.json',
        ],
      },
    },
  ],
}
