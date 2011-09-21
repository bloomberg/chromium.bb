# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'target_defaults': {
    'conditions': [
      ['toolkit_uses_gtk == 1 or use_wayland == 1', {
        'include_dirs': [
          '<(DEPTH)/third_party/angle/include',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'surface',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/gfx/gl/gl.gyp:gl',
        '<(DEPTH)/ui/ui.gyp:ui',
      ],
      'sources': [
        'accelerated_surface_linux.cc',
        'accelerated_surface_linux.h',
        'accelerated_surface_mac.cc',
        'accelerated_surface_mac.h',
        'accelerated_surface_wayland.cc',
        'accelerated_surface_wayland.h',
        'io_surface_support_mac.cc',
        'io_surface_support_mac.h',
        'surface_export.h',
        'transport_dib.h',
        'transport_dib_android.cc',
        'transport_dib_linux.cc',
        'transport_dib_mac.cc',
        'transport_dib_win.cc',
      ],
      'defines': [
        'SURFACE_IMPLEMENTATION',
      ],
      'conditions': [
        ['use_wayland == 1', {
          'sources/': [
            ['exclude', 'accelerated_surface_linux.cc'],
            ['exclude', 'accelerated_surface_linux.h'],
          ],
        }],
      ],
    },
  ],
}
