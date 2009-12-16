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
    ],
    # TODO(rlp): remove this after fixing signed / unsigned issues in
    # command buffer code and tests.
    'target_conditions': [
      ['OS == "mac"',
        {
          'xcode_settings': {
            'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO'
          },
        },
      ],
    ],
  },
  'targets': [
    {
      'target_name': 'standalone',
      'type': 'executable',
      'dependencies': [
        '../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../<(fcolladadir)/fcollada.gyp:fcollada',
        '../../<(jpegdir)/libjpeg.gyp:libjpeg',
        '../../<(pngdir)/libpng.gyp:libpng',
        '../../<(zlibdir)/zlib.gyp:zlib',
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../../native_client/src/shared/imc/imc.gyp:google_nacl_imc',
        '../compiler/technique/technique.gyp:o3dTechnique',
        '../core/core.gyp:o3dCore',
        '../core/core.gyp:o3dCorePlatform',
        '../utils/utils.gyp:o3dUtils',
      ],
      'sources': [
        'standalone.cc',
      ],
      'conditions' : [
        ['renderer == "gl"',
          {
            'dependencies': [
              '../build/libs.gyp:cg_libs',
              '../build/libs.gyp:gl_libs',
            ],
          },
        ],
        ['renderer == "gles2"',
          {
            'dependencies': [
              '../build/libs.gyp:cg_libs',
              '../build/libs.gyp:gles2_libs',
            ],
          },
        ],
        ['OS == "mac"',
          {
            'include_dirs': [
              '../../third_party/glew/files/include',
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/AGL.framework',
                '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                '$(SDKROOT)/System/Library/Frameworks/GLUT.framework',
                '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                '../../third_party/cg/files/mac/Cg.framework',
                '../../third_party/glew/files/lib/libMacStaticGLEW.a',
              ],
            },
          },
        ],
        ['OS == "win"',
          {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'rpcrt4.lib',
                  '../../<(cgdir)/lib/cg.lib',
                  '../../<(cgdir)/lib/cgGL.lib',
                ],
                # Set /SUBSYSTEM:WINDOWS for unit_tests.exe, since
                # it is a windows app.
                'SubSystem': '2',
                # Don't optimize away unreferenced symbols when
                # linking.  If we didn't do this, then none of the
                # tests would auto-register.
                'OptimizeReferences': '1',
              },
            },
            # We switch it to console post-build so that we have a
            # windows app that can output to the console and still
            # open windows.
            'msvs_postbuild':
              'editbin /SUBSYSTEM:CONSOLE $(OutDir)/$(TargetFileName)',
          },
        ],
        ['OS == "win" and renderer == "d3d9"',
          {
            'include_dirs': [
              '"$(DXSDK_DIR)/Include"',
            ],
            'link_settings': {
              'libraries': [
                '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
                'd3d9.lib',
                '"$(DXSDK_DIR)/Lib/x86/DxErr.lib"',
              ],
            },
          },
        ],
        ['OS == "win" and renderer == "gl"',
          {
            'dependencies': [
              '../build/libs.gyp:gl_libs',
            ],
          },
        ],
        ['OS == "win" and renderer == "gles2"',
          {
            'dependencies': [
              '../build/libs.gyp:gles2_libs',
            ],
          },
        ],
        ['OS == "linux"',
          {
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
