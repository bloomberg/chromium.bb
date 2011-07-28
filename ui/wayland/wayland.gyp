# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'wayland',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../build/linux/system.gyp:wayland',
        '../ui.gyp:ui',
      ],
      'include_dirs': [
        '.',
        '../..',
        'events',
      ],
      'sources': [
        'events/wayland_event.h',
        'wayland_buffer.cc',
        'wayland_buffer.h',
        'wayland_cursor.cc',
        'wayland_cursor.h',
        'wayland_display.cc',
        'wayland_display.h',
        'wayland_input_device.cc',
        'wayland_input_device.h',
        'wayland_message_pump.cc',
        'wayland_message_pump.h',
        'wayland_screen.cc',
        'wayland_screen.h',
        'wayland_shm_buffer.cc',
        'wayland_shm_buffer.h',
        'wayland_widget.h',
        'wayland_window.cc',
        'wayland_window.h',
      ],
    }
  ],
}
