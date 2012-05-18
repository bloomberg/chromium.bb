# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'common_sources': [
      'nc_thread.c',
      'nc_mutex.c',
      'nc_condvar.c',
      'nc_semaphore.c',
      'nc_token.c',
      'nc_init_irt.c',
      '../valgrind/dynamic_annotations.c',
    ],
  },
  'targets' : [
    {
      'target_name': 'pthread_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libpthread.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'sources': ['<@(common_sources)']
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'pthreadb_private_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libpthread_private.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'sources': [
          '<@(common_sources)',
          'nc_init_private.c',
          '../irt/irt_blockhook.c',
          '../irt/irt_cond.c',
          '../irt/irt_mutex.c',
          '../irt/irt_sem.c',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        'pthread_lib'
      ],
    },
  ],
}
