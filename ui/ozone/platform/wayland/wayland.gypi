# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_wayland',
    ],
    'internal_ozone_platform_unittest_deps': [
      'ozone_platform_wayland_unittests',
    ],
    'internal_ozone_platforms': [
      'wayland'
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_wayland',
      'type': 'static_library',
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../../third_party/wayland-protocols/wayland-protocols.gyp:xdg_shell_protocol',
        '../../third_party/wayland/wayland.gyp:wayland_client',
        '../events/platform/events_platform.gyp:events_platform',
      ],
      'sources': [
        'client_native_pixmap_factory_wayland.cc',
        'client_native_pixmap_factory_wayland.h',
        'ozone_platform_wayland.cc',
        'ozone_platform_wayland.h',
        'wayland_display.cc',
        'wayland_display.h',
        'wayland_object.h',
        'wayland_surface_factory.cc',
        'wayland_surface_factory.h',
        'wayland_window.cc',
        'wayland_window.h',
      ],
    },
    {
      'target_name': 'ozone_platform_wayland_unittests',
      'type': 'none',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '../../testing/gmock.gyp:gmock',
        '../../third_party/wayland-protocols/wayland-protocols.gyp:xdg_shell_protocol',
        '../../third_party/wayland/wayland.gyp:wayland_server',
        '../gfx/gfx.gyp:gfx_test_support',
      ],
      'export_dependent_settings': [
        '../../skia/skia.gyp:skia',
        '../../testing/gmock.gyp:gmock',
        '../../third_party/wayland-protocols/wayland-protocols.gyp:xdg_shell_protocol',
        '../../third_party/wayland/wayland.gyp:wayland_server',
        '../gfx/gfx.gyp:gfx_test_support',
      ],
      'direct_dependent_settings': {
        'sources': [
          'fake_server.cc',
          'fake_server.h',
          'mock_platform_window_delegate.cc',
          'wayland_display_unittest.cc',
          'wayland_surface_factory_unittest.cc',
          'wayland_window_unittest.cc',
        ],
      },
    },
  ],
}
