# -*- python -*-
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

{
  'variables': {
    'conditions': [
      ['OS=="linux"', {
        'syscall_handler': [
          'linux/nacl_syscall_impl.c'
        ],
      }],
      ['OS=="mac"', {
        'syscall_handler': [
          'linux/nacl_syscall_impl.c'
        ],
      }],
      ['OS=="win"', {
        'syscall_handler': [
          'win/nacl_syscall_impl.c'
        ],
        'msvs_cygwin_shell': 0,
      }],
    ],
  },
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'sel',
      'type': 'static_library',
      'sources': [
        'dyn_array.c',
        'env_cleanser.c',
        'nacl_all_modules.c',
        'nacl_app_thread.c',
        'nacl_bottom_half.c',
        'nacl_check.c',
        'nacl_closure.c',
        'nacl_globals.c',
        'nacl_memory_object.c',
        'nacl_sync_queue.c',
        'nacl_syscall_common.c',
        'nacl_syscall_hook.c',
        'sel_addrspace.c',
        'sel_ldr.c',
        'sel_ldr-inl.c',
        'sel_ldr_standard.c',
        'sel_load_image.c',
        'sel_mem.c',
        'sel_util.c',
        'sel_util-inl.c',
        'web_worker_stub.c',
      ],
      'sources!': [
         '<(syscall_handler)',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        'gio',
        '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:ncvalidate',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
      ],
      'conditions': [
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [
            'arch/x86/service_runtime_x86.gyp:service_runtime_x86',
          ],
        }],
        ['target_arch == "ia32"', {
          'dependencies': [
            'arch/x86_32/service_runtime_x86_32.gyp:service_runtime_x86_32',
          ],
        }],
        ['target_arch == "x64"', {
          'dependencies': [
            'arch/x86_64/service_runtime_x86_64.gyp:service_runtime_x86_64',
          ],
        }],
        # TODO(gregoryd): move arm-specific stuff into a separate gyp file.
        ['target_arch=="arm"', {
          'sources': [
            'arch/arm/nacl_app.c',
            'arch/arm/nacl_switch_to_app.c',
            'arch/arm/sel_rt.c',
            'arch/arm/nacl_tls.c',
            'arch/arm/sel_ldr.c',
            'arch/arm/sel_addrspace_arm.c',
            'arch/arm/sel_validate_image.c',
            'arch/arm/nacl_switch.S',
            'arch/arm/nacl_syscall.S',
            'arch/arm/springboard.S',
            'arch/arm/tramp.S',
            'arch/arm/nacl_tls_tramp.S',
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            'linux/sel_memory.c',
            'linux/nacl_thread_nice.c',
          ],
          'conditions': [
            ['target_arch=="ia32" or target_arch=="x64"', {
              'sources': [
                'linux/x86/nacl_ldt.c',
                'linux/x86/sel_segments.c',
              ],
            }],
            ['target_arch=="arm"', {
              'sources': [
                'linux/arm/sel_segments.c',
              ],
            }],
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'osx/nacl_ldt.c',
            'linux/sel_memory.c',
            'linux/x86/sel_segments.c',
            'osx/nacl_thread_nice.c',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'win/nacl_ldt.c',
            'win/sel_memory.c',
            'win/sel_segments.c',
            'win/nacl_thread_nice.c',
          ],
        }],
        ['nacl_standalone==0 and OS=="win"', {
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:ldrhandle',
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'nacl_syscall_handler',
          'inputs': [
            'nacl_syscall_handlers_gen2.py',
            '<(syscall_handler)',
          ],
          'action':
            # TODO(gregoryd): find out how to generate a file
            # in such a location that can be found in both
            # NaCl and Chrome builds.
            ['<@(python_exe)', 'nacl_syscall_handlers_gen2.py', '-c',
             '-f', 'Video',
             '-f', 'Audio',
             '-f', 'Multimedia',
             '-i', '<@(syscall_handler)',
             '-o', '<@(_outputs)'],

          'msvs_cygwin_shell': 0,
          'msvs_quote_cmd': 0,
          'outputs': [
            '<(INTERMEDIATE_DIR)/nacl_syscall_handlers.c',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Creating nacl_syscall_handlers.c',
        },
      ],
    }, {
      'target_name': 'gio',
      'type': 'static_library',
      'sources': [
        'gio.c',
        'gio_mem.c',
        'gprintf.c',
        'gio_mem_snapshot.c',
      ],
    }, {
      'target_name': 'container',
      'type': 'static_library',
      'sources': [
        'generic_container/container.c',
      ],
    }, {
      'target_name': 'expiration',
      'type': 'static_library',
      'sources': [
        'expiration.c',
      ],
    }, {
      'target_name': 'nacl_xdr',
      'type': 'static_library',
      'sources': [
        'fs/xdr.c',
        'fs/obj_proxy.c',
      ],
    },
    {
      'target_name': 'sel_ldr',
      'type': 'executable',
      # TODO(gregoryd): currently building sel_ldr without SDL
      'dependencies': [
        'expiration',
        'sel',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
      ],
      'sources': [
        'sel_main.c',
      ],
    },

    # TODO(bsy): no tests are built; see build.scons
  ],
}
