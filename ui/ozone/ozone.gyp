# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'external_ozone_platform_files': [],
    'external_ozone_platform_deps': [],
  },
  'targets': [
    {
      'target_name': 'ozone',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/events/events.gyp:events',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<@(external_ozone_platform_deps)',
      ],
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'sources': [
        'ozone_platform.cc',
        'ozone_platform.h',
        'ozone_switches.cc',
        'ozone_switches.h',
        'platform/dri/ozone_platform_dri.cc',
        'platform/dri/ozone_platform_dri.h',
        'platform/test/ozone_platform_test.cc',
        'platform/test/ozone_platform_test.h',
        '<@(external_ozone_platform_files)',
      ],
      'conditions': [
        ['ozone_platform != "dri"', {
          'sources/': [
            ['exclude', '^platform/dri/'],
          ]
        }],
        ['ozone_platform != "test"', {
          'sources/': [
            ['exclude', '^platform/test/'],
          ]
        }],
      ]
    },
  ],
}
