# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'installer',
      'type': 'none',
      'conditions': [
        [ '"<!(which dpkg-buildpackage || true)" != ""',
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
