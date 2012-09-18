# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'viewer',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
      ],
      'sources': [
        'viewer_host_win.cc',
        'viewer_host_win.h',
        'viewer_ipc_server.cc',
        'viewer_ipc_server.h',
        'viewer_main.cc',
        'viewer_message_generator.cc',
        'viewer_message_generator.h',
        'viewer_process.cc',
        'viewer_process.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}

