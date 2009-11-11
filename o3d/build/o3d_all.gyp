# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'O3D_All',
      'type': 'none',
      'dependencies': [
        '../../<(antlrdir)/antlr.gyp:*',
        '../../<(fcolladadir)/fcollada.gyp:*',
        '../../<(jpegdir)/libjpeg.gyp:*',
        '../../<(pngdir)/libpng.gyp:*',
        '../../<(zlibdir)/zlib.gyp:*',
        '../compiler/technique/technique.gyp:o3dTechnique',
        '../converter/converter.gyp:o3dConverter',
        '../core/core.gyp:o3dCore',
        '../core/core.gyp:o3dCorePlatform',
        '../documentation/documentation.gyp:*',
        '../import/archive.gyp:o3dArchive',
        '../import/import.gyp:o3dImport',
        '../installer/installer.gyp:installer',
        '../plugin/idl/idl.gyp:o3dPluginIdl',
        '../plugin/idl/idl.gyp:o3dNpnApi',
        '../plugin/plugin.gyp:npo3dautoplugin',
        '../samples/samples.gyp:*',
        '../tests/selenium/selenium.gyp:*',
        '../tests/tests.gyp:unit_tests',
        '../utils/utils.gyp:o3dUtils',
      ],
      'conditions': [
        ['OS=="win"',
          {
            'dependencies': [
              '../plugin/plugin.gyp:o3d_host',
            ],
          },
        ],
        ['OS=="mac"',
          {
            'dependencies': [
              '../../breakpad/breakpad.gyp:breakpad',
            ],
          },
        ],
        ['renderer=="cb"',
          {
            'dependencies': [
              '../gpu_plugin/gpu_plugin.gyp:*',
            ],
          },
        ],
      ],
    },
  ],
}
