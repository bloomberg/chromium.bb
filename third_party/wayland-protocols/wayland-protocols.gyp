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
    {
      'target_name': 'linux_dmabuf_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/linux-dmabuf-unstable-v1-client-protocol.h',
        'include/protocol/linux-dmabuf-unstable-v1-server-protocol.h',
        'protocol/linux-dmabuf-protocol.c',
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
    {
      'target_name': 'scaler_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/scaler-client-protocol.h',
        'include/protocol/scaler-server-protocol.h',
        'protocol/scaler-protocol.c',
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
    {
      'target_name': 'secure_output_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/secure-output-unstable-v1-client-protocol.h',
        'include/protocol/secure-output-unstable-v1-server-protocol.h',
        'protocol/secure-output-protocol.c',
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
    {
      'target_name': 'alpha_compositing_protocol',
      'type': 'static_library',
      'dependencies' : [
        '../wayland/wayland.gyp:wayland_util',
      ],
      'sources': [
        'include/protocol/alpha-compositing-unstable-v1-client-protocol.h',
        'include/protocol/alpha-compositing-unstable-v1-server-protocol.h',
        'protocol/alpha-compositing-protocol.c',
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
