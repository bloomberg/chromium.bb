# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/accelerometer
      'target_name': 'ui_accelerometer',
      'type': '<(component)',
      'dependencies': [
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'UI_ACCELEROMETER_IMPLEMENTATION',
      ],
      'sources' : [
        'accelerometer_types.cc',
        'accelerometer_types.h',
        'ui_accelerometer_export.h',
      ],
      'include_dirs': [
        '../..',
      ],
    },
  ],
}
