# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '..',
      '../..',
      '../../<(gtestdir)',
    ],
  },
  'targets': [
    {
      'target_name': 'o3dConverter',
      'type': 'executable',
      'dependencies': [
        '../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../<(fcolladadir)/fcollada.gyp:fcollada',
        '../../<(jpegdir)/libjpeg.gyp:libjpeg',
        '../../<(pngdir)/libpng.gyp:libpng',
        '../../<(zlibdir)/zlib.gyp:zlib',
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../compiler/technique/technique.gyp:o3dTechnique',
        '../core/core.gyp:o3dCore',
        '../core/core.gyp:o3dCorePlatform',
        '../import/archive.gyp:o3dArchive',
        '../import/import.gyp:o3dImport',
        '../serializer/serializer.gyp:o3dSerializer',
        '../utils/utils.gyp:o3dUtils',
        '../build/libs.gyp:cg_libs',
      ],
      'sources': [
        'cross/buffer_stub.cc',
        'cross/buffer_stub.h',
        'cross/converter.cc',
        'cross/converter.h',
        'cross/converter_main.cc',
        'cross/draw_element_stub.h',
        'cross/effect_stub.h',
        'cross/param_cache_stub.h',
        'cross/primitive_stub.h',
        'cross/render_surface_stub.h',
        'cross/renderer_stub.cc',
        'cross/renderer_stub.h',
        'cross/sampler_stub.h',
        'cross/stream_bank_stub.h',
        'cross/texture_stub.cc',
        'cross/texture_stub.h',
      ],
      'conditions' : [
        ['OS == "mac"',
          {
            'sources': [
              'mac/converter_main.mm',
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework',
                '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                '../../../<(cgdir)/Cg.framework',
              ],
            },
          },
        ],
        ['OS == "win"',
          {
            'actions': [
              {
                'action_name': 'd3dx9_36_dll_copy',
                'inputs': [
                  '$(DXSDK_DIR)/runtime/x86/d3dx9d_36.dll',
                ],
                'outputs': [
                  '<(PRODUCT_DIR)/d3dx9_36.dll',
                ],
                'action': [
                  'cp',
                  '-f',
                  '<@(_inputs)',
                  '<@(_outputs)',
                ],
              },
            ],
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'rpcrt4.lib',
                  '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
                  '../../<(cgdir)/lib/cg.lib',
                  '../../<(cgdir)/lib/cgGL.lib',
                ],
                # Set /SUBSYSTEM:CONSOLE for converter.exe, since
                # it is a console app.
                'SubSystem': '1',
              },
            },
          },
        ],
      ],
    },
  ],
}
