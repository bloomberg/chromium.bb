# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      [ 'OS=="linux"', {
        # Link to system .so since we already use it due to GTK.
        'use_system_bzip2%': 1,
      }, {  # OS!="linux"
        'use_system_bzip2%': 0,
      }],
    ],
  },
  'conditions': [
    ['use_system_bzip2==0', {
      'targets': [
        {
          'target_name': 'bzip2',
          'type': '<(library)',
          'defines': ['BZ_NO_STDIO'],
          'msvs_guid': '2A70CBF0-847E-4E3A-B926-542A656DC7FE',
          'sources': [
            'blocksort.c',
            'bzlib.c',
            'bzlib.h',
            'bzlib_private.h',
            'compress.c',
            'crctable.c',
            'decompress.c',
            'huffman.c',
            'randtable.c',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
          'conditions': [
            ['OS=="win"', {
              'product_name': 'libbzip2',
            }, {  # else: OS!="win"
              'product_name': 'chrome_bz2',
            }],
          ],
        },
      ],
    }, {
      'targets': [
        {
          'target_name': 'bzip2',
          'type': 'settings',

          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_LIBBZ2',
            ],
          },

          # There aren't any pkg-config files for libbz2
          'link_settings': {
            'libraries': [
              '-lbz2',
            ],
          },
        },
      ]
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
