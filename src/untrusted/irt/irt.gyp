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
      'irt_dev_exception_handling.c',
# support_srcs
      '../pthread/nc_mutex.c',
      '../pthread/nc_condvar.c',
      '../pthread/nc_token.c',
      '../nacl/sys_private.c',
    ],
    'irt_nonbrowser': [
      'irt_interfaces.c',
      'irt_core_resource.c',
    ],
    'irt_browser': [
      'irt_interfaces_ppapi.c',
      'irt_entry_ppapi.c',
      'irt_ppapi.c',
      'irt_manifest.c',
      'irt_nameservice.c',
      'irt_random.c',
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
           'cflags': ['--pnacl-allow-translate',
                      '-arch', 'arm'],
           'asflags': ['-arch', 'arm'],
           'link_flags': [
             '-Wl,--section-start,.rodata=<(NACL_IRT_DATA_START)',
             '-Wl,-Ttext=<(NACL_IRT_TEXT_START)',
             '--pnacl-allow-native',
             '-arch', 'arm',
             '-Wt,-mtls-use-call',
             # TODO(olonho): rethink
             '-L<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm',
           ],
         },
       ],
      ],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
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
