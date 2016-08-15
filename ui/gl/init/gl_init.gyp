# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    {
      'target_name': 'gl_init',
      'type': '<(component)',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../gfx/gfx.gyp:gfx',
        '../../gfx/gfx.gyp:gfx_geometry',
        '../gl.gyp:gl',
      ],
      'defines': [
        'GL_INIT_IMPLEMENTATION',
      ],
      'sources': [
        'gl_initializer.h',
        'gl_initializer_android.cc',
        'gl_initializer_mac.cc',
        'gl_initializer_ozone.cc',
        'gl_initializer_win.cc',
        'gl_initializer_x11.cc',
        'gl_factory.cc',
        'gl_factory.h',
        'gl_factory_android.cc',
        'gl_factory_mac.cc',
        'gl_factory_ozone.cc',
        'gl_factory_win.cc',
        'gl_factory_x11.cc',
        'gl_init_export.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'dwmapi.dll',
              ],
              'AdditionalDependencies': [
                'dwmapi.lib',
              ],
            },
          },
          'link_settings': {
            'libraries': [
              '-ldwmapi.lib',
            ],
          },
        }],
        ['use_x11 == 1', {
          'dependencies': [
            '../../gfx/x/gfx_x11.gyp:gfx_x11',
          ],
        }],
        ['use_ozone==1', {
          'dependencies': [
            '../../ozone/gl/ozone_gl.gyp:ozone_gl',
            '../../ozone/ozone.gyp:ozone',
            '../../ozone/ozone.gyp:ozone_base',
          ],
        }],
      ],
    },
  ],
}
