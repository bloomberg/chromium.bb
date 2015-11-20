# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'battor_agent',
      'type': 'executable',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        'battor_agent_lib',
        '../../device/serial/serial.gyp:device_serial',
        '../../third_party/mojo/mojo_public.gyp:mojo_environment_standalone',
        '../../third_party/mojo/mojo_public.gyp:mojo_public',
      ],
      'sources': [
        'battor_agent_bin.cc',
      ],
    },
    {
      'target_name': 'battor_agent_lib',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'battor_agent.cc',
        'battor_agent.h',
        'battor_connection.cc',
        'battor_connection.h',
        'battor_error.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../device/serial/serial.gyp:device_serial',
      ]
    },
  ],
}
