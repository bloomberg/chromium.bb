# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    # Depend on this target to set the O3D_IN_CHROME macro. This also works for
    # indirectly dependent Chromium projects that do not include the O3D
    # common.gypi.
    {
      'target_name': 'o3d_in_chrome',
      'type': 'none',
      'conditions': [
        ['o3d_in_chrome == "True"',
          {
            'all_dependent_settings': {
              'defines': [
                'O3D_IN_CHROME',
              ],  # 'defines'
            },  # 'all_dependent_settings'
          },
        ],
      ],  # 'conditions'
    },
  ],
}
