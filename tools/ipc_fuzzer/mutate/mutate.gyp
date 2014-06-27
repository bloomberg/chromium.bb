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
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'mutate.cc',
        'rand_util.h',
        'rand_util.cc',
      ],
      'conditions': [
        ['asan==1', {
          'cflags!': [
            # Compiling mutate.cc with ASan takes too long, see
            # http://crbug.com/360158.
            '-fsanitize=address',
          ],
        }],
      ],
      'include_dirs': [
        '../../..',
      ],
      'defines': [
        'USE_CUPS',
      ],
    },
    {
      'target_name': 'ipc_fuzzer_generate',
      'type': 'executable',
      'dependencies': [
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'generate.cc',
        'rand_util.h',
        'rand_util.cc',
      ],
      'conditions': [
        ['asan==1', {
          'cflags!': [
            # Compiling generate.cc with ASan takes too long, see
            # http://crbug.com/360158.
            '-fsanitize=address',
          ],
        }],
      ],
      'include_dirs': [
        '../../..',
      ],
      'defines': [
        'USE_CUPS',
      ],
    },
    {
      'target_name': 'ipc_message_util',
      'type': 'executable',
      'dependencies': [
        '../../../third_party/re2/re2.gyp:re2',
        '../message_lib/message_lib.gyp:ipc_message_lib',
      ],
      'sources': [
        'message_util.cc',
      ],
      'include_dirs': [
        '../../..',
      ],
      'defines': [
        'USE_CUPS',
      ],
    },
  ],
}
