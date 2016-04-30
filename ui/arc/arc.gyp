# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN: //ui/arc:arc
      'target_name': 'arc',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../gfx/gfx.gyp:gfx_geometry',
        '../message_center/message_center.gyp:message_center',
        '../../base/base.gyp:base',
        '../../url/url.gyp:url_lib',
        '../../skia/skia.gyp:skia',
        '../../components/components.gyp:arc_mojo_bindings',
        '../../components/components.gyp:signin_core_account_id',
      ],
      'sources': [
        'notification/arc_notification_manager.cc',
        'notification/arc_notification_manager.h',
        'notification/arc_notification_item.cc',
        'notification/arc_notification_item.h',
      ],
    },
    {
      # GN: //ui/arc:ui_arc_unittests
      'target_name': 'ui_arc_unittests',
      'type': '<(gtest_target_type)',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../components/components.gyp:arc_test_support',
        '../../mojo/mojo_edk.gyp:mojo_system_impl',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
        '../../mojo/mojo_public.gyp:mojo_message_pump_lib',
        '../../testing/gtest.gyp:gtest',
        '../message_center/message_center.gyp:message_center_test_support',
        'arc',
      ],
      'sources': [
        'notification/arc_notification_manager_unittest.cc',
        'test/run_all_unittests.cc',
      ],
    },
  ],
}
