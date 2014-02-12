# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'display',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'display_constants.h',
      ],
    },
  ],
}
