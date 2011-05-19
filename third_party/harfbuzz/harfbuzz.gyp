# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['use_harfbuzz_ng==0', {
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
          'dependencies': [
            '../../build/linux/system.gyp:freetype2',
          ],
        },
      ],
    }, {  # else, use new harfbuzz
      'targets': [
        {
          # Make the 'harfbuzz' target just shim through to the harfbuzz-ng
          # one.
          'target_name': 'harfbuzz',
          'type': 'settings',
          'dependencies': [
            '../harfbuzz-ng/harfbuzz.gyp:harfbuzz'
          ],
          'export_dependent_settings': [
            '../harfbuzz-ng/harfbuzz.gyp:harfbuzz'
          ],
        }
      ]
    }]
  ]
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
