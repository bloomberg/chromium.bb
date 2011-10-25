# Copyright (c) 2009 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          'XP_UNIX',
        ],
        'dependencies': [
          'posix/nonnacl_util_posix.gyp:nonnacl_util_posix',
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
        'sources': [
          'posix/sel_ldr_launcher_posix.cc',
          'osx/get_plugin_dirname.mm',
        ],
        'link_settings': {
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          ],
        },
      }],
      ['OS=="win"', {
        'defines': [
          'XP_WIN',
          'WIN32',
          '_WINDOWS'
        ],
        'sources': [
          'win/sel_ldr_launcher_win.cc',
        ],
      }],
    ],
  },
}
