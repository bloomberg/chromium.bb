# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'cr_elements_resources',
      'type': 'none',
      'dependencies': [
        'network/compiled_resources.gyp:*',
        'policy/compiled_resources.gyp:*',
      ],
    },
  ]
}
