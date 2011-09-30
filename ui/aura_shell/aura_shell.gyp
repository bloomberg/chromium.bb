# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    {
      'target_name': 'aura_shell',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../build/temp_gyp/googleurl.gyp:googleurl',
        '../../net/net.gyp:net',
        '../../skia/skia.gyp:skia',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
        '../base/strings/ui_strings.gyp:ui_strings',
        '../gfx/compositor/compositor.gyp:compositor',
        '../ui.gyp:ui',
        '../ui.gyp:ui_resources',
        '../ui.gyp:ui_resources_standard',
        '../aura/aura.gyp:aura',
        '../../views/views.gyp:views',
      ],
      'sources': [
        # All .cc, .h under views, except unittests
        'desktop_background_view.cc',
        'desktop_background_view.h',
        'desktop_layout_manager.cc',
        'desktop_layout_manager.h',
        'desktop_window.cc',
        'launcher/launcher_view.cc',
        'launcher/launcher_view.h',
        'launcher/launcher_button.cc',
        'launcher/launcher_button.h',
        'shell_factory.h',
        'shell_window_ids.h',
        'status_area_view.cc',
        'status_area_view.h',
        'toplevel_frame_view.cc',
        'toplevel_frame_view.h',
      ],
    },
    {
      'target_name': 'aura_shell_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../build/temp_gyp/googleurl.gyp:googleurl',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
        '../ui.gyp:gfx_resources',
        '../ui.gyp:ui',
        '../ui.gyp:ui_resources',
        '../ui.gyp:ui_resources_standard',
        'aura_shell',
      ],
      'sources': [
        'run_all_unittests.cc',

        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
      ],
    },
    {
      'target_name': 'aura_shell_exe',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../chrome/chrome.gyp:packed_resources',
        '../../skia/skia.gyp:skia',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
        '../../views/views.gyp:views',
        '../aura/aura.gyp:aura',
        '../ui.gyp:gfx_resources',
        '../ui.gyp:ui',
        '../ui.gyp:ui_resources',
        '../ui.gyp:ui_resources_standard',
        'aura_shell',
      ],
      'sources': [
        'aura_shell_main.cc',
        'examples/bubble.cc',
        'examples/example_factory.h',
        'examples/toplevel_window.cc',
        'examples/toplevel_window.h',
        'examples/window_type_launcher.cc',
        'examples/window_type_launcher.h',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
      ],
    },
    # It's convenient for aura_shell developers to be able to build all
    # compositor and aura targets from within this solution.
    {
      'target_name': 'buildbot_targets',
      'type': 'none',
      'dependencies': [
        'aura_shell_exe',
        '../aura/aura.gyp:*',
        '../gfx/compositor/compositor.gyp:*',
        '../../views/views.gyp:views',
        '../../views/views.gyp:views_aura_desktop',
        '../../views/views.gyp:views_desktop',
        '../../views/views.gyp:views_desktop_lib',
        '../../views/views.gyp:views_unittests',
      ],
    },     
  ],
}
