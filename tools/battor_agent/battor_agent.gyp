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
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../device/serial/serial.gyp:device_serial',
      ]
    },
  ],
}
