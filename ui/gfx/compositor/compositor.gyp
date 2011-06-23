# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'sources/': [
      ['exclude', '_(gl|win)\\.(cc?)$'],
    ],
    'conditions': [
      ['os_posix == 1 and OS != "mac"', {
        'sources/': [['include', '_(gl)\\.cc$'],]
      }],
      ['OS == "win"', {
        'sources/': [['include', '_(win)\\.cc$'],]
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'compositor',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/gfx/gl/gl.gyp:gl',
        '<(DEPTH)/ui/ui.gyp:ui_gfx',
      ],
      'sources': [
        'compositor.cc',
        'compositor.h',
        'compositor_gl.cc',
        'compositor_win.cc',
        'layer.cc',
        'layer.h',
        'layer_animator.cc',
        'layer_animator.h',
      ],
      'conditions': [
        ['os_posix == 1 and OS != "mac"', {
          'sources!': [
            'compositor.cc',
          ],
        }],
        ['OS == "win" and views_compositor == 1', {
          'sources!': [
            'compositor.cc',
          ],
          # TODO(sky): before we make this real need to remove
          # IDR_BITMAP_BRUSH_IMAGE.
          'dependencies': [
            '<(DEPTH)/ui/ui.gyp:gfx_resources',
          ],
          'link_settings': {
            'libraries': [
              '-ld3d10.lib',
              '-ld3dx10d.lib',
              '-ldxerr.lib',
              '-ldxguid.lib',
            ]
          },
        }],
        ['OS == "win" and views_compositor == 0', {
          'sources/': [
            ['exclude', '^compositor_win.cc'],
          ],
        }],
      ],
    },
  ],
}
