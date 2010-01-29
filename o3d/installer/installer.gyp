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
  'targets': [
    {
      'target_name': 'installer',
      'type': 'none',
      'conditions': [
        ['OS=="win"',
          {
            'dependencies': [
              'win/installer.gyp:installer',
              'win/installer.gyp:extras_installer',
            ],
          },
        ],
        ['OS=="mac"',
          {
            'dependencies': [
              'mac/installer.gyp:disk_image',
            ],
          },
        ],
        ['OS=="linux"',
          {
            'dependencies': [
              'linux/installer.gyp:installer',
            ],
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
