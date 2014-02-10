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
      'irt_futex.c',
      'irt_mutex.c',
      'irt_cond.c',
      'irt_sem.c',
      'irt_tls.c',
      'irt_blockhook.c',
      'irt_clock.c',
      'irt_dev_getpid.c',
      'irt_exception_handling.c',
      'irt_dev_list_mappings.c',
      'irt_nameservice.c',
      'irt_random.c',
# support_srcs
      # We also get nc_init_private.c, nc_thread.c and nc_tsd.c via
      # #includes of .c files.
      '../pthread/nc_mutex.c',
      '../pthread/nc_condvar.c',
      '../nacl/sys_private.c',
      '../valgrind/dynamic_annotations.c',
    ],
    'irt_nonbrowser': [
      'irt_interfaces.c',
      'irt_core_resource.c',
    ],
    'irt_browser': [
      'irt_interfaces_ppapi.c',
      'irt_ppapi.c',
      'irt_manifest.c',
    ],
  },
  'targets': [
    {
      'target_name': 'irt_core_raw_nexe',
      'type': 'none',
      'variables': {
        'nexe_target': 'irt_core_raw',
        # These out_* fields override the default filenames, which
        # include a "_newlib" suffix and places them in the target
        # directory.
        'out_newlib64': '<(SHARED_INTERMEDIATE_DIR)/irt_core_x86_64_raw.nexe',
        'out_newlib32': '<(SHARED_INTERMEDIATE_DIR)/irt_core_x86_32_raw.nexe',
        'out_newlib_arm': '<(SHARED_INTERMEDIATE_DIR)/irt_core_arm_raw.nexe',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_irt': 1,
      },
      'sources': ['<@(irt_sources)', '<@(irt_nonbrowser)'],
      'link_flags': [
        '-lsrpc',
        '-limc_syscalls',
        '-lplatform',
        '-lgio',
        '-lm',
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
      'target_name': 'irt_core_nexe',
      'type': 'none',
      'dependencies': [
        'irt_core_raw_nexe',
        '<(DEPTH)/native_client/src/tools/tls_edit/tls_edit.gyp:tls_edit#host',
      ],
      'conditions': [
        ['target_arch=="arm"', {
          'actions': [
            {
              'action_name': 'tls_edit_irt_arm',
              'message': 'Patching TLS for irt_core (arm)',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '<(PRODUCT_DIR)/tls_edit',
                '<(SHARED_INTERMEDIATE_DIR)/irt_core_arm_raw.nexe',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/irt_core_newlib_arm.nexe',
              ],
              'action': ['<@(_inputs)', '<@(_outputs)'],
            },
          ],
        }],
        ['target_arch!="arm"', {
          'actions': [
            {
              'action_name': 'tls_edit_irt_x86-64',
              'message': 'Patching TLS for irt_core (x86-64)',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '<(PRODUCT_DIR)/tls_edit',
                '<(SHARED_INTERMEDIATE_DIR)/irt_core_x86_64_raw.nexe',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/irt_core_newlib_x64.nexe',
              ],
              'action': ['<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'tls_edit_irt_x86-32',
              'message': 'Patching TLS for irt_core (x86-32)',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '<(PRODUCT_DIR)/tls_edit',
                '<(SHARED_INTERMEDIATE_DIR)/irt_core_x86_32_raw.nexe',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/irt_core_newlib_x32.nexe',
              ],
              'action': ['<@(_inputs)', '<@(_outputs)'],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'irt_browser_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libirt_browser.a',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_irt': 1,
      },
      'sources': ['<@(irt_sources)', '<@(irt_browser)'],
      'include_dirs': ['../../../../ppapi'],
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
      ],
    },
  ],
}
