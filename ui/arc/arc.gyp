# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'arc',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../gfx/gfx.gyp:gfx_geometry',
        '../message_center/message_center.gyp:message_center',
        '../../base/base.gyp:base',
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
  ],
}
