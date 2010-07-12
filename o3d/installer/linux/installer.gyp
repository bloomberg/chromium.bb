# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'installer',
      'type': 'none',
      'conditions': [
        [ '"<!(whereis dpkg-buildpackage)" != "dpkg-buildpackage:"',
          {
            'dependencies': [
              'debian.in/debian.gyp:debian',
            ],
          },
        ],
      ],
    },
  ],
}
