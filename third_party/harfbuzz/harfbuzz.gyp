# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Set to 1 to use Harfbuzz-NG instead of Harfbuzz.
    # Under development: http://crbug.com/68551
    'use_harfbuzz_ng%': 0
  },
  'conditions': [
    ['use_harfbuzz_ng==0', {
      'includes': [
        '../../build/win_precompile.gypi',
      ],
      'targets': [
        {
          'target_name': 'harfbuzz',
          'type': 'static_library',
          'sources': [
            'contrib/harfbuzz-freetype.c',
            'contrib/harfbuzz-unicode.c',
            'contrib/harfbuzz-unicode-tables.c',
            'src/harfbuzz-buffer.c',
            'src/harfbuzz-stream.c',
            'src/harfbuzz-dump.c',
            'src/harfbuzz-gdef.c',
            'src/harfbuzz-gpos.c',
            'src/harfbuzz-gsub.c',
            'src/harfbuzz-impl.c',
            'src/harfbuzz-open.c',
            'src/harfbuzz-shaper.cpp',
            'src/harfbuzz-tibetan.c',
            'src/harfbuzz-khmer.c',
            'src/harfbuzz-indic.cpp',
            'src/harfbuzz-greek.c',
            'src/harfbuzz-hebrew.c',
            'src/harfbuzz-arabic.c',
            'src/harfbuzz-hangul.c',
            'src/harfbuzz-myanmar.c',
            'src/harfbuzz-thai.c',
          ],
          'include_dirs': [
            'contrib',
            'src',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'contrib',
              'src',
            ],
          },
          'conditions': [
            ['OS == "android"', {
              'dependencies': [
                '../../third_party/freetype/freetype.gyp:ft2',
              ],
            }, {  # OS != android
              'dependencies': [
                '../../build/linux/system.gyp:freetype2',
              ],
            }],
            ['clang == 1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  # "harfbuzz is in strict maintenace mode",
                  # see http://crbug.com/113708
                  '-Wno-pointer-sign',
                ],
              },
              'cflags': [
                '-Wno-pointer-sign',
              ],
            }],
          ],
        },
      ],
    }, {  # else, use new harfbuzz
      'targets': [
        {
          # Make the 'harfbuzz' target just shim through to the harfbuzz-ng
          # one.
          'target_name': 'harfbuzz',
          'type': 'none',
          'dependencies': [
            '../harfbuzz-ng/harfbuzz.gyp:harfbuzz-ng'
          ],
          'export_dependent_settings': [
            '../harfbuzz-ng/harfbuzz.gyp:harfbuzz-ng'
          ],
        }
      ]
    }]
  ]
}
