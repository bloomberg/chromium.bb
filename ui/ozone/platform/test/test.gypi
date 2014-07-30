# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_test',
    ],
    'internal_ozone_platforms': [
      'test'
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_test',
      'type': 'static_library',
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'ozone_platform_test.cc',
        'ozone_platform_test.h',
        'test_cursor_factory.cc',
        'test_cursor_factory.h',
        'test_event_factory.cc',
        'test_event_factory.h',
        'test_window.cc',
        'test_window.h',
        'test_window_manager.cc',
        'test_window_manager.h',
      ],
    },
  ],
}
