# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_wayland',
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
        "client_native_pixmap_factory_wayland.cc",
        "client_native_pixmap_factory_wayland.h",
        "ozone_platform_wayland.cc",
        "ozone_platform_wayland.h",
        "wayland_display.cc",
        "wayland_display.h",
        "wayland_object.h",
        "wayland_surface_factory.cc",
        "wayland_surface_factory.h",
        "wayland_window.cc",
        "wayland_window.h",
      ],
    },
  ],
}
