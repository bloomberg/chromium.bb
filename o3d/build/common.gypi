# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'antlrdir': 'third_party/antlr3',
    'breakpaddir': 'breakpad/src',
    'cb_service%': 'none',
    'fcolladadir': 'third_party/fcollada/files',
    'glewdir': 'third_party/glew/files',
    'gtestdir': 'testing/gtest/include',
    'jpegdir': 'third_party/libjpeg',
    'nacldir': 'third_party/native_client/googleclient',
    'nixysadir': 'third_party/nixysa',
    'npapidir': 'third_party/npapi',
    'pdiffdir': 'third_party/pdiff/files',
    'pngdir': 'third_party/libpng',
    'screenshotsdir': 'o3d_assets/tests/screenshots',
    'seleniumdir': 'third_party/selenium_rc/files',
    'skiadir': 'third_party/skia/include',
    'zlibdir': 'third_party/zlib',
    'o3d_in_chrome%': 0,
    'selenium_screenshots%': 0,
  },
  'target_defaults': {
    'defines': [
      'GYP_BUILD',  # Needed to make a change in base/types.h conditional.
    ],
    # This needs to be in a target_conditions block in order to successfully
    # override the xcode_settings in ../../build/common.gypi.
    # Something to do with evaluation order.
    'target_conditions': [
      ['OS=="mac"', {
          'xcode_settings': {
            'MACOSX_DEPLOYMENT_TARGET': '10.4',
            # TODO(maf): figure out proper fix for the following.
            # There is only one place in plugin_mac.mm which attempts
            # to use ObjC exception handling.
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
          },
      }],
      ['o3d_in_chrome == 1',
        {
          'defines': [
            'O3D_IN_CHROME',
          ],
        },
      ],
    ],
  },
  'conditions' : [
    ['OS == "win"',
      {
        'variables': {
          'renderer%': 'd3d9',
          'cgdir': 'third_party/cg/files/win',
          'swiftshaderdir': 'o3d-internal/third_party/swiftshader/files',
          'LIBRARY_SUFFIX': '.lib',
        },
        'target_defaults': {
          'defines': [
            '_CRT_SECURE_NO_WARNINGS',
            'OS_WIN',
            'UNICODE',
            'NACL_WINDOWS',
          ],
          # Disable warning: "'this' : used in base member initialization list."
          'msvs_disabled_warnings': [4355],
          'conditions': [
            ['renderer == "d3d9"',
              {
                'defines': [
                  'RENDERER_D3D9',
                ],
              },
            ],
            ['renderer == "gl"',
              {
                'defines': [
                  'RENDERER_GL',
                ],
              },
            ],
            ['renderer == "cb"',
              {
                'defines': [
                  'RENDERER_CB',
                ],
              },
            ],
          ],
        },
      },
    ],
    ['OS == "mac"',
      {
        'variables': {
          'renderer%': 'gl',
          'cgdir': 'third_party/cg/files/mac',
          'LIBRARY_SUFFIX': '.a',
        },
        'target_defaults': {
          'defines': [
            'OS_MACOSX',
            'UNICODE',
            'GTEST_NOT_MAC_FRAMEWORK_MODE',
            'NACL_OSX=1',
            'MAC_OS_X_VERSION_MIN_REQUIRED=MAC_OS_X_VERSION_10_4',
            'SK_BUILD_FOR_MAC',
          ],
          'configurations': {
            'Debug': {
              'xcode_settings': {
                'GCC_DEBUGGING_SYMBOLS': 'full',
        				'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
              },
            },
          },
          'xcode_settings': {
            'OTHER_CFLAGS': [
               '-fno-eliminate-unused-debug-symbols',
               '-mmacosx-version-min=10.4'],
            'WARNING_CFLAGS': ['-Wno-deprecated-declarations'],
            'WARNING_CXXFLAGS': ['-Wstrict-aliasing',
                                 '-Wno-deprecated',],
          },
          'conditions': [
            ['renderer == "gl"',
              {
                'defines': [
                  'RENDERER_GL',
                ],
              },
            ],
            ['renderer == "cb"',
              {
                'defines': [
                  'RENDERER_CB',
                ],
              },
            ],
          ],
        },
      },
    ],
    ['OS == "linux"',
      {
        'variables': {
          'renderer%': 'gl',
          'cgdir': 'third_party/cg/files/linux',
          'LIBRARY_SUFFIX': '.a',
        },
        'target_defaults': {
          'defines': [
            'LINUX',
            'MOZ_X11',
            'NACL_LINUX=1',
            'OS_LINUX',
            'SK_BUILD_FOR_UNIX',
            'UNICODE',
            'XP_UNIX',
          ],
          'cflags': [
            '-fvisibility=hidden',
            '-Wstrict-aliasing',
          ],
          'conditions': [
            ['renderer == "gl"',
              {
                'defines': [
                  'RENDERER_GL',
                ],
              },
            ],
            ['renderer == "cb"',
              {
                'defines': [
                  'RENDERER_CB',
                ],
              },
            ],
          ],
        },
      },
    ],
    ['cb_service == "d3d9"',
      {
        'target_defaults': {
          'defines': [
            'CB_SERVICE_D3D9',
          ],
        },
      },
    ],
    ['cb_service == "gl"',
      {
        'target_defaults': {
          'defines': [
            'CB_SERVICE_GL',
          ],
        },
      },
    ],
  ],
}
