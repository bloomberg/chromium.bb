# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'keyboard',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
        '../compositor/compositor.gyp:compositor',
        '../ui.gyp:ui',
      ],
      'defines': [
        'KEYBOARD_IMPLEMENTATION',
      ],
      'sources': [
        'keyboard_controller.cc',
        'keyboard_controller.h',
        'keyboard_controller_proxy.h',
        'keyboard_export.h',
        'keyboard_switches.cc',
        'keyboard_switches.h',
        'keyboard_util.cc',
        'keyboard_util.h',
      ]
    },
    {
      'target_name': 'keyboard_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../aura/aura.gyp:aura',
        '../aura/aura.gyp:aura_test_support',
        '../compositor/compositor.gyp:compositor',
        '../ui.gyp:run_ui_unittests',
        '../ui.gyp:ui',
        'keyboard',
      ],
      'sources': [
        'keyboard_controller_unittest.cc',
      ],
    },
  ],
}
