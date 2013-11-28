# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipc_fuzzer',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ipc/ipc.gyp:ipc',
        '../../chrome/chrome.gyp:common',
      ],
      'sources': [
        'ipc_fuzzer_main.cc',
      ],
      'include_dirs': [
        '../..',
      ],
    },
  ],
}

