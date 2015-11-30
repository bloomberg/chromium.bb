# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'xdg_shell_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/xdg-shell-unstable-v5-client-protocol.h',
        'include/protocol/xdg-shell-unstable-v5-server-protocol.h',
        'protocol/xdg-shell-protocol.c',
      ],
      'include_dirs': [
        'include/protocol',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include/protocol',
        ],
      },
    },
  ],
}
