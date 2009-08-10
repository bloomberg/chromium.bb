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
  },
  'targets': [
    {
      'target_name': 'unit_tests',
      'type': 'executable',
      'dependencies': [
        '../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../<(fcolladadir)/fcollada.gyp:fcollada',
        '../../<(jpegdir)/libjpeg.gyp:libjpeg',
        '../../<(pngdir)/libpng.gyp:libpng',
        '../../<(zlibdir)/zlib.gyp:zlib',
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../../native_client/src/shared/imc/imc.gyp:google_nacl_imc',
        '../compiler/technique/technique.gyp:o3dTechnique',
        '../compiler/technique/technique.gyp:o3dTechniqueTest',
        '../core/core.gyp:o3dCore',
        '../core/core.gyp:o3dCorePlatform',
        '../core/core.gyp:o3dCoreTest',
        '../import/import.gyp:o3dImport',
        '../import/archive.gyp:o3dArchiveTest',
        '../import/import.gyp:o3dImportTest',
        '../serializer/serializer.gyp:o3dSerializerTest',
        '../statsreport/statsreport.gyp:o3dStatsReportTest',
        '../utils/utils.gyp:o3dUtils',
        '../utils/utils.gyp:o3dUtilsTest',
      ],
      'sources': [
        'common/cross/test_utils.cc',
        'common/cross/main.cc',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/bitmap_test',
          'files': [
            "bitmap_test/5kx5k.dds",
            "bitmap_test/5kx5k.jpg",
            "bitmap_test/5kx5k.png",
            "bitmap_test/5kx5k.tga",
            "bitmap_test/dds-dxt1-256x256-alpha.dds",
            "bitmap_test/dds-dxt1-256x256-mipmap.dds",
            "bitmap_test/dds-dxt1-256x256.dds",
            "bitmap_test/dds-dxt3-256x256-alpha.dds",
            "bitmap_test/dds-dxt3-256x256-mipmap.dds",
            "bitmap_test/dds-dxt5-256x256-alpha.dds",
            "bitmap_test/dds-dxt5-256x256-mipmap.dds",
            "bitmap_test/gif-256x256-interlaced.gif",
            "bitmap_test/gif-256x256.gif",
            "bitmap_test/jpeg-256x256.jpg",
            "bitmap_test/png-20x14-4bit-palette.png",
            "bitmap_test/png-256x256-24bit-interlaced.png",
            "bitmap_test/png-256x256-24bit.png",
            "bitmap_test/png-256x256-32bit.png",
            "bitmap_test/png-256x256-8bit-palette-alpha.png",
            "bitmap_test/png-256x256-8bit-palette.png",
            "bitmap_test/png-2x2-24bit-drawimage-src.png",
            "bitmap_test/png-4x4-24bit-drawimage-argb8-src.png",
            "bitmap_test/png-4x4-24bit-drawimage-src.png",
            "bitmap_test/png-8x4-24bit-drawimage-argb8-dest.png",
            "bitmap_test/png-8x4-24bit-drawimage-dest.png",
            "bitmap_test/png-8x8-24bit-drawimage-src.png",
            "bitmap_test/test_source.psd",
            "bitmap_test/tga-256x256-24bit.tga",
            "bitmap_test/tga-256x256-32bit.tga",
          ],
        },
      ],
      'conditions' : [
        ['OS == "mac"',
          {
            'sources': [
              'common/mac/testing_common.mm',
            ],
            'include_dirs': [
              '../../third_party/glew/files/include',
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              ],
            },
          },
        ],
        ['OS == "win"',
          {
            'sources': [
              'common/win/testing_common.cc',
              'common/win/testing_common.h',
            ],
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
            'sources': [
              'common/win/dxcapture.cc',
            ],
            'include_dirs': [
              '$(DXSDK_DIR)/Include',
            ],
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  '"$(DXSDK_DIR)/Lib/x86/DxErr9.lib"',
                  '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
                  'd3d9.lib',
                ],
              },
            },
          },
        ],
        ['OS == "linux"',
          {
            'sources': [
              'common/linux/testing_common.cc',
            ],
            'include_dirs': [
              '../../third_party/glew/files/include',
            ],
          },
        ],
      ],
    },
  ],
}
