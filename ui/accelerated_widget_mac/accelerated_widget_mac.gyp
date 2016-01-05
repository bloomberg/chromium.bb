# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/accelerated_widget_mac
      'target_name': 'accelerated_widget_mac',
      'type': '<(component)',
      'sources': [
        'accelerated_widget_mac.h',
        'accelerated_widget_mac.mm',
        'accelerated_widget_mac_export.h',
        'display_link_mac.cc',
        'display_link_mac.h',
        'io_surface_context.h',
        'io_surface_context.mm',
        'window_resize_helper_mac.cc',
        'window_resize_helper_mac.h',
      ],
      'defines': [
        'ACCELERATED_WIDGET_MAC_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
        '<(DEPTH)/ui/events/events.gyp:events_base',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
        ],
      },
    },
  ],
}
