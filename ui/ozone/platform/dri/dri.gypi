# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_dri',
    ],
    'internal_ozone_platforms': [
      'dri'
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_dri',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../build/linux/system.gyp:dridrm',
        '../../skia/skia.gyp:skia',
        '../base/ui_base.gyp:ui_base',
        '../display/display.gyp:display_types',
        '../display/display.gyp:display_util',
        '../events/events.gyp:events',
        '../events/ozone/events_ozone.gyp:events_ozone_evdev',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'sources': [
        'chromeos/display_mode_dri.cc',
        'chromeos/display_mode_dri.h',
        'chromeos/display_snapshot_dri.cc',
        'chromeos/display_snapshot_dri.h',
        'chromeos/native_display_delegate_dri.cc',
        'chromeos/native_display_delegate_dri.h',
        'cursor_factory_evdev_dri.cc',
        'cursor_factory_evdev_dri.h',
        'dri_buffer.cc',
        'dri_buffer.h',
        'dri_surface.cc',
        'dri_surface.h',
        'dri_surface_factory.cc',
        'dri_surface_factory.h',
        'dri_util.cc',
        'dri_util.h',
        'dri_vsync_provider.cc',
        'dri_vsync_provider.h',
        'dri_wrapper.cc',
        'dri_wrapper.h',
        'hardware_display_controller.cc',
        'hardware_display_controller.h',
        'ozone_platform_dri.cc',
        'ozone_platform_dri.h',
        'screen_manager.cc',
        'screen_manager.h',
      ],
    },
  ],
}
