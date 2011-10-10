# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  ######################################################################
  'includes': [
    'build/common.gypi',
  ],
  ######################################################################
  'targets': [
    {
      'target_name': 'hello_world_nexe',
      'type': 'none',
      'dependencies': [
        'tools.gyp:prep_toolchain',
        'src/untrusted/nacl/nacl.gyp:nacl_lib',
        'src/untrusted/irt/irt.gyp:irt_core_nexe'
      ],
      'variables': {
        'nexe_target': 'hello_world',
        'build_glibc': 0,
        'build_newlib': 1,
        'sources': [
          'tests/hello_world/hello_world.c',
        ],
      },
    },
  ],
}
