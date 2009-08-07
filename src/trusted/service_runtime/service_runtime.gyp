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
  'target_defaults': {
    'rules': [
    {
      'rule_name': 'cygwin_assembler',
      'msvs_cygwin_shell': 0,
      'extension': 'S',
      'inputs': [
        '..\\..\\third_party\\gnu_binutils\\files\\as.exe',
      ],
      'outputs': [
        '<(INTERMEDIATE_DIR)\\<(RULE_INPUT_ROOT).obj',
      ],
      'action':
        # TODO(gregoryd): find a way to pass all other 'defines' that exist for the target.
        ['cl /E /I..\..\..\.. /DNACL_BLOCK_SHIFT=5 /DNACL_WINDOWS=1 <(RULE_INPUT_PATH) | <@(_inputs) -defsym @feat.00=1 -o <(INTERMEDIATE_DIR)\\<(RULE_INPUT_ROOT).obj'],
      'message': 'Building assembly file <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],

  },
  'targets': [
    {
      'target_name': 'sel',
      'type': 'static_library',
      'sources': [
        'dyn_array.c',
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
        'sel_ldr_standard.c',
        'sel_load_image.c',
        'sel_mem.c',
        'sel_util.c',
        'sel_validate_image.c',
        'web_worker_stub.c',
      ],
      'sources!': [
         '<(syscall_handler)',
      ],
      'conditions': [
        # TODO(petr):
        # * introduce build_arch, build_subarch, target_subarch in the same way
        #   as BUILD_ARCH and TARGET_ARCH in scons (see SConstruct fror details).
        # * rename ia32 to x86_32, ia64 to x86_64 as in scons
        # * introduce buildplatform, targetplatform, platform flags in gyp
        ['target_arch == "ia32"', {
          'sources': [
            'arch/x86/nacl_app.c',
            'arch/x86/nacl_ldt_x86.c',
            'arch/x86/nacl_switch_to_app.c',
            'arch/x86/sel_rt.c',
            'arch/x86/nacl_tls.c',
            'arch/x86/sel_ldr_x86.c',
            'arch/x86/sel_addrspace_x86.c',
            'arch/x86_32/nacl_switch.S',
            'arch/x86_32/nacl_syscall.S',
            'arch/x86_32/springboard.S',
            'arch/x86_32/tramp.S',
          ],
          'dependencies': [
            'tramp_gen',
            'springboard_gen'
          ],
          'include_dirs': [
            '<(INTERMEDIATE_DIR)',
          ],
          'actions': [
            {
              'action_name': 'header_gen',
              'conditions': [
                ['OS=="win"', {
                  'msvs_cygwin_shell': 0,
                }],
              ],
              'inputs': [
                '<(PRODUCT_DIR)/tramp_gen',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/gen/native_client/src/trusted/service_runtime/arch/x86/tramp_data.h',
              ],
              'action':
                ['<@(_inputs)', '>', '<@(_outputs)'],
              'process_outputs_as_sources': 1,
              'message': 'Creating tramp_data.h',
            },
            {
              'action_name': 'sheader_gen',
              'conditions': [
                ['OS=="win"', {
                  'msvs_cygwin_shell': 0,
                }],
              ],
              'inputs': [
                '<(PRODUCT_DIR)/springboard_gen',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/gen/native_client/src/trusted/service_runtime/arch/x86/springboard_data.h',
              ],
              'action':
                ['<@(_inputs)', '>', '<@(_outputs)'],
              'process_outputs_as_sources': 1,
              'message': 'Creating springboard_data.h',
            },
          ],
        }],
        ['target_arch=="arm"', {
          'sources': [
            'arch/arm/nacl_app.c',
            'arch/arm/nacl_switch_to_app.c',
            'arch/arm/sel_rt.c',
            'arch/arm/nacl_tls.c',
            'arch/arm/sel_ldr.c',
            'arch/arm/sel_addrspace_arm.c',
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
          ],
          'conditions': [
            ['target_arch=="ia32"', {
              'sources': [
                'linux/x86/nacl_ldt.c',
                'linux/x86/sel_segments.c',
              ],
            }],
            ['target_arch=="arm"', {
              'soruces': [
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
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'win/nacl_ldt.c',
            'win/sel_memory.c',
            'win/sel_segments.c',
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'nacl_syscall_handler',
          'inputs': [
            'nacl_syscall_handlers_gen2.py',
            'nacl_syscall_handlers_gen3.py',
            '<(syscall_handler)',
          ],
          'conditions': [
            ['OS=="win"', {
              'msvs_cygwin_shell': 0,
            }],
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/nacl_syscall_handlers.c',
          ],
          'action':
             # TODO(gregoryd): find out how to generate a file in such a location that can be found in both NaCl and Chrome builds.
             ['python', 'nacl_syscall_handlers_gen3.py', '-c', '-f', '"Video|Audio|Multimedia"', '<', '<@(syscall_handler)', '>', '<@(_outputs)'],
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
      'target_name': 'tramp_gen',
      'type': 'executable',
      'sources': [
        'arch/x86_32/tramp.S',
        'arch/x86_32/tramp_gen.c',
      ],
    },
    {
      'target_name': 'springboard_gen',
      'type': 'executable',
      'sources': [
        'arch/x86_32/springboard.S',
        'arch/x86_32/springboard_gen.c',
      ],
    },
    # TODO(bsy): no tests are built; see build.scons
  ],
}
