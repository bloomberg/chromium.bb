# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ui_chromeos_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/resources',
      },
      'actions': [
        {
          'action_name': 'ui_chromeos_resources',
          'variables': {
            'grit_grd_file': 'resources/ui_chromeos_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },
    {
      'target_name': 'ui_chromeos_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/strings',
      },
      'actions': [
        {
          'action_name': 'generate_ui_chromeos_strings',
          'variables': {
            'grit_grd_file': 'ui_chromeos_strings.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },
    {
      'target_name': 'ui_chromeos',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
        '../events/events.gyp:events',
        '../events/events.gyp:gesture_detection',
        '../wm/wm.gyp:wm',
        'ui_chromeos_resources',
        'ui_chromeos_strings',
      ],
      'defines': [
        'UI_CHROMEOS_IMPLEMENTATION',
      ],
      'sources': [
        'network/network_icon.cc',
        'network/network_icon.h',
        'network/network_icon_animation.cc',
        'network/network_icon_animation.h',
        'network/network_icon_animation_observer.h',
        'touch_exploration_controller.cc',
        'touch_exploration_controller.h',
        'user_activity_power_manager_notifier.cc',
        'user_activity_power_manager_notifier.h',
      ],
    },
  ],
}
