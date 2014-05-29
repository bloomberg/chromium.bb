# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    'platform/dri/dri_surface_factory_unittest.cc',
    'platform/dri/dri_surface_unittest.cc',
    'platform/dri/hardware_display_controller_unittest.cc',
    'platform/dri/screen_manager_unittest.cc',
    'platform/dri/test/mock_dri_surface.cc',
    'platform/dri/test/mock_dri_surface.h',
    'platform/dri/test/mock_dri_wrapper.cc',
    'platform/dri/test/mock_dri_wrapper.h',
    'platform/dri/test/mock_surface_generator.cc',
    'platform/dri/test/mock_surface_generator.h',
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
