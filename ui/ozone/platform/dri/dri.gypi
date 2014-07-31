# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_dri',
    ],
    'internal_ozone_platform_unittest_deps': [
      'ozone_platform_dri_unittests',
    ],
    'internal_ozone_platforms': [
      'dri',
    ],
    'use_drm_atomic_flip%': 0,
  },
  'targets': [
    {
      'target_name': 'ozone_platform_dri',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../build/linux/system.gyp:libdrm',
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
        'crtc_state.cc',
        'crtc_state.h',
        'cursor_factory_evdev_dri.cc',
        'cursor_factory_evdev_dri.h',
        'dri_console_buffer.cc',
        'dri_console_buffer.h',
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
        'dri_window.cc',
        'dri_window.h',
        'dri_wrapper.cc',
        'dri_wrapper.h',
        'hardware_display_controller.cc',
        'hardware_display_controller.h',
        'ozone_platform_dri.cc',
        'ozone_platform_dri.h',
        'scoped_drm_types.cc',
        'scoped_drm_types.h',
        'screen_manager.cc',
        'screen_manager.h',
        'scanout_buffer.h',
        'virtual_terminal_manager.cc',
        'virtual_terminal_manager.h',
      ],
      'conditions': [
        ['use_drm_atomic_flip==1', {
          'sources': [
            'hardware_display_plane.cc',
            'hardware_display_plane.h',
            'hardware_display_plane_manager.cc',
            'hardware_display_plane_manager.h',
          ],
        }],
      ],      
    },
    {
      'target_name': 'ozone_platform_dri_unittests',
      'type': 'none',
      'dependencies': [
        '../../build/linux/system.gyp:libdrm',
        '../../skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx_geometry',
        'ozone_platform_dri',
      ],
      'export_dependent_settings': [
        '../../build/linux/system.gyp:libdrm',
        '../../skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'direct_dependent_settings': {
        'sources': [
          'dri_surface_factory_unittest.cc',
          'dri_surface_unittest.cc',
          'hardware_display_controller_unittest.cc',
          'screen_manager_unittest.cc',
          'test/mock_dri_wrapper.cc',
          'test/mock_dri_wrapper.h',
        ],
      },
    },
  ],
}
