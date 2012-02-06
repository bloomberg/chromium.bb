# Copyright (c) 2009 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'dependencies': [
      'nonnacl_util_launcher.gyp:nonnacl_util_launcher',
    ],
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          'XP_UNIX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'XP_MACOSX',
          'XP_UNIX',
          'TARGET_API_MAC_CARBON=1',
          'NO_X11',
          'USE_SYSTEM_CONSOLE',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'XP_WIN',
          'WIN32',
          '_WINDOWS'
        ],
      }],
    ],
  },
}
