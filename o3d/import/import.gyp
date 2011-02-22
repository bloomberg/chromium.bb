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
      '../../<(cgdir)/include',
      '../../<(glewdir)/include',
      '../../<(gtestdir)',
    ],
  },
  'targets': [
    {
      'target_name': 'o3dImport',
      'type': 'static_library',
      'dependencies': [
        'archive.gyp:o3dArchive',
        '../../<(antlrdir)/antlr.gyp:antlr3c',
        '../../<(fcolladadir)/fcollada.gyp:fcollada',
        '../../<(jpegdir)/libjpeg.gyp:libjpeg',
        '../../<(pngdir)/libpng.gyp:libpng',
        '../../<(zlibdir)/zlib.gyp:zlib',
        '../compiler/technique/technique.gyp:o3dTechnique',
        '../build/libs.gyp:cg_libs',
      ],
      'sources': [
        'cross/collada_conditioner.cc',
        'cross/collada_conditioner.h',
        'cross/collada.cc',
        'cross/collada.h',
        'cross/collada_zip_archive.cc',
        'cross/collada_zip_archive.h',
        'cross/file_output_stream_processor.cc',
        'cross/file_output_stream_processor.h',
        'cross/precompile.h',
        'cross/tar_generator.cc',
        'cross/tar_generator.h',
        'cross/targz_generator.cc',
        'cross/targz_generator.h',
        'cross/zip_archive.cc',
        'cross/zip_archive.h',
      ],
      'conditions' : [
        ['OS == "win"',
          {
            'msvs_system_include_dirs': [
              '$(DXSDK_DIR)/Include',
            ],
            'sources': [
              'win/collada_conditioner_win.cc',
            ],
          },
        ],
        ['OS == "mac"',
          {
            'sources': [
              'mac/collada_conditioner_mac.mm',
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
        ['OS == "linux"',
          {
            'sources': [
              'linux/collada_conditioner_linux.cc',
            ],
            'include_dirs': [
              '../../third_party/glew/files/include',
            ],
          },
        ],
      ],
    },
    {
      'target_name': 'o3dSerializationObjects',
      'type': 'static_library',
      'sources': [
        'cross/camera_info.cc',
        'cross/camera_info.h',
        'cross/destination_buffer.cc',
        'cross/destination_buffer.h',
        'cross/json_object.cc',
        'cross/json_object.h',
      ],
    },
    {
      'target_name': 'o3dImportTest',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'cross/tar_generator_test.cc',
          'cross/targz_generator_test.cc',
        ],
      },
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/unittest_data',
          'files': [
            'test_data/crate.dae',
            'test_data/crate.jpg',
            'test_data/rock01.tga',
            'test_data/rock02.tga',
          ],
        },
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
