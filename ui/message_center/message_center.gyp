# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'message_center',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../skia/skia.gyp:skia',
        '../base/strings/ui_strings.gyp:ui_strings',
        '../compositor/compositor.gyp:compositor',
        '../ui.gyp:ui',
        '../ui.gyp:ui_resources',
        '../views/views.gyp:views',
      ],
      'defines': [
        'MESSAGE_CENTER_IMPLEMENTATION',
      ],
      'sources': [
        'base_format_view.cc',
        'base_format_view.h',
        'message_bubble_base.cc',
        'message_bubble_base.h',
        'message_center.cc',
        'message_center.h',
        'message_center_bubble.cc',
        'message_center_bubble.h',
        'message_center_constants.cc',
        'message_center_constants.h',
        'message_center_export.h',
        'message_popup_bubble.cc',
        'message_popup_bubble.h',
        'message_simple_view.cc',
        'message_simple_view.h',
        'message_view.cc',
        'message_view.h',
        'message_view_factory.cc',
        'message_view_factory.h',
        'notification_list.cc',
        'notification_list.h',
        'notification_view.cc',
        'notification_view.h',
        'quiet_mode_bubble.cc',
        'quiet_mode_bubble.h',
      ],
    },
    {
      'target_name': 'message_center_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
        '../ui.gyp:ui',
        'message_center',
      ],
      'sources': [
        'notification_list_unittest.cc',
      ],
    },
  ],
}
