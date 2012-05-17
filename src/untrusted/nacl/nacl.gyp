# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'sources_for_standard_interfaces': [
      'clock.c',
      'clock_getres.c',
      'clock_gettime.c',
      'close.c',
      'dup.c',
      '_exit.c',
      'fstat.c',
      'getdents.c',
      'getpid.c',
      'gettimeofday.c',
      'lock.c',
      'lseek.c',
      'malloc.c',
      'mmap.c',
      'munmap.c',
      'nanosleep.c',
      'nacl_interface_query.c',
      'nacl_read_tp.c',
      'open.c',
      'read.c',
      'pthread_initialize_minimal.c',
      'pthread_stubs.c',
      'sbrk.c',
      'sched_yield.c',
      'stacktrace.c',
      'start.c',
      'stat.c',
      'sysconf.c',
      'tls.c',
      'write.c',
    ],
    'sources_for_nacl_extensions': [
      'gc_hooks.c',
      'nacl_irt.c',
      'nacl_tls_get.c',
      'nacl_tls_init.c',
    ],
    'imc_syscalls': [
      'imc_accept.c',
      'imc_connect.c',
      'imc_makeboundsock.c',
      'imc_mem_obj_create.c',
      'imc_recvmsg.c',
      'imc_sendmsg.c',
      'imc_socketpair.c',
      'nameservice.c',
    ],
  },

  'targets' : [
    {
      'target_name': 'nacl_lib',
      'type': 'none',
      'dependencies': [
        'nacl_lib_newlib',
        'nacl_lib_glibc',
      ],
      'conditions': [
        # NOTE: We do not support glibc on arm yet.
        ['target_arch!="arm"', {
           'dependencies': [
             'nacl_lib_glibc'
           ]
         }],
      ],
    },

    {
      'target_name': 'nacl_lib_glibc',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl.a',
        'build_glibc': 1,
        'build_newlib': 0,
      },
      'sources': ['<@(sources_for_nacl_extensions)'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_lib_newlib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl.a',
        'build_glibc': 0,
        'build_newlib': 1,
      },
      'sources': [
        '<@(sources_for_nacl_extensions)',
        '<@(sources_for_standard_interfaces)',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_dynacode_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_dyncode.a',
        'build_glibc': 1,
        'build_newlib': 1,
      },
      'sources': ['dyncode.c'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_dynacode_private_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_dyncode_private.a',
        'build_glibc': 0,
        'build_newlib': 1,
      },
      'sources': ['dyncode_private.c'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'imc_syscalls_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libimc_syscalls.a',
        'build_glibc': 1,
        'build_newlib': 1,
      },
      'sources': ['<@(imc_syscalls)'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
  ],
}
