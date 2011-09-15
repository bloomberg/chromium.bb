# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'aura',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../gfx/compositor/compositor.gyp:compositor',
        '../ui.gyp:gfx_resources',
        '../ui.gyp:ui',
        '../ui.gyp:ui_resources',
      ],
      'defines': [
        'AURA_IMPLEMENTATION',
      ],
      'sources': [
        'desktop_host.h',
        'desktop_host_linux.cc',
        'desktop_host_win.cc',
        'desktop_host_win.h',
        'desktop.cc',
        'desktop.h',
        'event.cc',
        'event.h',
        'event_win.cc',
        'event_x.cc',
        'focus_manager.cc',
        'focus_manager.h',
        'hit_test.h',
        'layout_manager.h',
        'root_window.cc',
        'root_window.h',
        'window.cc',
        'window.h',
        'window_delegate.h',
        'window_manager.cc',
        'window_manager.h',
      ],
    },
    {
      'target_name': 'aura_demo',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
        '../gfx/compositor/compositor.gyp:compositor',
        '../ui.gyp:gfx_resources',
        '../ui.gyp:ui',
        '../ui.gyp:ui_resources',
        'aura',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'demo/demo_main.cc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
      ],
    },
    {
      'target_name': 'aura_unittests',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/chrome/chrome.gyp:packed_resources',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/ui/ui.gyp:gfx_resources',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
        'aura',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'window_unittest.cc',
        'run_all_unittests.cc',
        'test_suite.cc',
        'test_suite.h',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
      ],
    },
  ],
}
