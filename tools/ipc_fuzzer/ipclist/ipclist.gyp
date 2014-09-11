# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipclist',
      'type': 'executable',
      'defines': [
        'FULL_SAFE_BROWSING',
      ],
      'dependencies': [
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'ipclist.cc',
      ]
    }
  ]
}
