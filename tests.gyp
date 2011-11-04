# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  ######################################################################
  'includes': [
    'build/common.gypi',
  ],
  ######################################################################
  'conditions': [
    ['target_arch!="arm"', {
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
        {
          'target_name': 'test_hello_world_nexe',
          'type': 'none',
          'dependencies': [
            'hello_world_nexe',
            'src/trusted/service_runtime/service_runtime.gyp:sel_ldr',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                'src/trusted/service_runtime/service_runtime.gyp:sel_ldr64',
              ],
            }],
          ],
          'actions': [
            {
              'action_name': 'test build',
              'msvs_cygwin_shell': 0,
              'description': 'Testing NACL build',
              'inputs': [],
              'outputs': ['hello_world.out'],
              'action': [
                '>(python_exe)',
                '<(DEPTH)/native_client/build/test_build.py',
                '<(PRODUCT_DIR)',
                'hello_world',
                '<(target_arch)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
