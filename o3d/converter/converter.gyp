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
        ['renderer == "gl"',
          {
            'dependencies': [
              '../build/libs.gyp:cg_libs',
            ],
          },
        ],
        ['OS == "mac"',
          {
            'postbuilds': [
              {
                'variables': {
                  # Define install_name in a variable ending in _path
                  # so that gyp understands it's a path and performs proper
                  # relativization during dict merging.
                  'install_name_path': 'mac/converter_install_name.sh',
                },
                'postbuild_name': 'Fix Framework Paths',
                'action': ['<(install_name_path)'],
              },
            ],
            'sources': [
              'mac/converter_main.mm',
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework',
                '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
              ],
            },
          },
        ],
        ['OS == "linux"',
          {
            'link_settings': {
              'libraries': [
                '-lGL',
              ],
            },
          },
        ],
        ['OS == "win"',
          {
            'dependencies': [
              '../build/libs.gyp:dx_dll',
              '../build/libs.gyp:cg_libs',
            ],
            'link_settings': {
              'libraries': [
                '-lrpcrt4.lib',
              ],
            },
            'msvs_settings': {
              'VCLinkerTool': {
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

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
