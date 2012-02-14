# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="linux"', {
      'defines': [
        'XP_UNIX',
      ],
      'sources': [
        'posix/get_plugin_dirname.cc',
        'posix/sel_ldr_launcher_posix.cc',
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
        'osx/get_plugin_dirname.mm',
        'posix/sel_ldr_launcher_posix.cc',
      ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-pedantic',  # import is a gcc extension
        ],
      },
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
}
