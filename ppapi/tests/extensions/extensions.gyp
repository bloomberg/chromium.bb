# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../ppapi/ppapi_nacl_test_common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ppapi_tests_extensions_socket',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_tests_extensions_socket',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'test_data/ppapi/tests/extensions/socket',
        'sources': [
          'socket/test_socket.cc',
          '<(DEPTH)/ppapi/tests/test_utils.cc',
          '<(DEPTH)/ppapi/tests/test_utils.h',
        ],
        'test_files': [
          'socket/controller.js',
          'socket/index.html',
          'socket/main.js',
          'socket/manifest.json',
        ],
      },
    },
  ],
}
