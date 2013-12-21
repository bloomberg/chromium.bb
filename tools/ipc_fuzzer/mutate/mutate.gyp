# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ipc_fuzzer_mutate',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../chrome/chrome.gyp:common',
        '../../../ipc/ipc.gyp:ipc',
        '../../../ppapi/ppapi_internal.gyp:ppapi_ipc',
        '../../../skia/skia.gyp:skia',
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'mutate.cc',
      ],
      'include_dirs': [
        '../../..',
      ],
    },
    {
      'target_name': 'ipc_fuzzer_generate',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../chrome/chrome.gyp:common',
        '../../../ipc/ipc.gyp:ipc',
        '../../../ppapi/ppapi_internal.gyp:ppapi_ipc',
        '../../../skia/skia.gyp:skia',
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'generate.cc',
      ],
      'include_dirs': [
        '../../..',
      ],
    },
  ],
}
