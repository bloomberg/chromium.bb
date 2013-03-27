# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'irt_sources': [
# irt_support_sources
      'irt_entry.c',
      'irt_malloc.c',
      'irt_private_pthread.c',
      'irt_private_tls.c',
# irt_common_interfaces
      'irt_basic.c',
      'irt_fdio.c',
      'irt_filename.c',
      'irt_memory.c',
      'irt_dyncode.c',
      'irt_thread.c',
      'irt_mutex.c',
      'irt_cond.c',
      'irt_sem.c',
      'irt_tls.c',
      'irt_blockhook.c',
      'irt_clock.c',
      'irt_dev_getpid.c',
      'irt_dev_exception_handling.c',
      'irt_nameservice.c',
      'irt_random.c',
# support_srcs
      # We also get nc_init_private.c, nc_thread.c and nc_tsd.c via
      # #includes of .c files.
      '../pthread/futex.c',
      '../pthread/nc_mutex.c',
      '../pthread/nc_condvar.c',
      '../nacl/sys_private.c',
      '../valgrind/dynamic_annotations.c',
    ],
    'irt_nonbrowser': [
      'irt_interfaces.c',
      'irt_core_entry.c',
      'irt_core_resource.c',
    ],
    'irt_browser': [
      'irt_interfaces_ppapi.c',
      'irt_entry_ppapi.c',
      'irt_ppapi.c',
      'irt_manifest.c',
    ],
  },
  'targets': [
    {
      'target_name': 'irt_core_nexe',
      'type': 'none',
      'variables': {
        'nexe_target': 'irt_core',
        'build_glibc': 0,
        'build_newlib': 1,
      },
      'sources': ['<@(irt_sources)', '<@(irt_nonbrowser)'],
      'link_flags': [
        '-lsrpc',
        '-limc_syscalls',
        '-lplatform',
        '-lgio',
        '-lm',
      ],
      'conditions': [
        # See comment in native_client/src/untrusted/irt/nacl.scons
        # regarding -Ttext-segment. Also see
        # http://code.google.com/p/nativeclient/issues/detail?id=2691.
        ['target_arch!="arm"',
         {
           'link_flags': [
             '-Wl,--section-start,.rodata=<(NACL_IRT_DATA_START)',
             '-Wl,-Ttext-segment=<(NACL_IRT_TEXT_START)',
           ]
         }, { # target_arch == "arm"
           'sources': ['<@(irt_sources)',
                       'aeabi_read_tp.S'],
           'asflags': ['-arch', 'arm'],
           'link_flags': [
             '-Wl,--section-start,.rodata=<(NACL_IRT_DATA_START)',
             '-Wl,-Ttext=<(NACL_IRT_TEXT_START)',
             # TODO(olonho): rethink
             '-L<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm',
             '-Wt,-mtls-use-call',
             '-Wl,--pnacl-irt-link',
           ],
         },
       ],
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:srpc_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'irt_browser_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libirt_browser.a',
        'build_glibc': 0,
        'build_newlib': 1,
      },
      'sources': ['<@(irt_sources)', '<@(irt_browser)'],
      'include_dirs': ['../../../../ppapi'],
      'conditions': [
        ['target_arch == "x64" or target_arch == "ia32"', {
          'link_flags': [
             '-r',
             '-nostartfiles',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
      ],
    },
  ],
}
