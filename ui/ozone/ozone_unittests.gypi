# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    'platform/dri/hardware_display_controller_unittest.cc',
    'platform/dri/dri_surface_factory_unittest.cc',
    'platform/dri/dri_surface_unittest.cc',
  ],
  'conditions': [
    ['ozone_platform_dri == 1', {
      'dependencies': [
        '../build/linux/system.gyp:dridrm',
        '../ui/ozone/ozone.gyp:ozone',
      ],
    }, {
      'sources/': [
        ['exclude', '^platform/dri/'],
      ],
    }],
  ],
}
