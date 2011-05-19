# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'sources/': [
      ['exclude', '/(cocoa|gtk|win)/'],
      ['exclude', '_(cocoa|gtk|linux|mac|posix|win|x)\\.(cc|mm?)$'],
      ['exclude', '/(gtk|win|x11)_[^/]*\\.cc$'],
    ],
    'conditions': [
      ['toolkit_uses_gtk == 1', {
        'sources/': [
          ['include', '/gtk/'],
          ['include', '_(gtk|linux|posix|skia|x)\\.cc$'],
          ['include', '/(gtk|x11)_[^/]*\\.cc$'],
        ],
      }],
      ['OS=="mac"', {'sources/': [
        ['include', '/cocoa/'],
        ['include', '_(cocoa|mac|posix)\\.(cc|mm?)$'],
      ]}, { # else: OS != "mac"
        'sources/': [
          ['exclude', '\\.mm?$'],
        ],
      }],
      ['OS=="win"',
        {'sources/': [
          ['include', '_(win)\\.cc$'],
          ['include', '/win/'],
          ['include', '/win_[^/]*\\.cc$'],
      ]}],
    ],
  },
  'targets': [
    {
      'target_name': 'surface',
      'type': 'static_library',
      'msvs_guid': '6C99567D-6F02-43B5-BB35-D8E3F8D0D6D2',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/gfx/gl/gl.gyp:gl',
        '<(DEPTH)/ui/ui.gyp:ui_gfx',
      ],
      'sources': [
        'accelerated_surface_mac.cc',
        'accelerated_surface_mac.h',
        'io_surface_support_mac.cc',
        'io_surface_support_mac.h',
        'transport_dib.h',
        'transport_dib_linux.cc',
        'transport_dib_mac.cc',
        'transport_dib_win.cc',
      ],
    },
  ],
}
