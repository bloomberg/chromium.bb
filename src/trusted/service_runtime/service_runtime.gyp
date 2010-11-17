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
    'variables':{
      'target_base': 'none',
    },
    'target_conditions': [
      ['OS=="linux" or OS=="mac"', {
        'cflags': [
          '-fexceptions',
        ],
        'cflags_cc' : [
          '-frtti',
        ]
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',      # -fexceptions
          'GCC_ENABLE_CPP_RTTI': 'YES',            # -frtti
        }
      }],
      ['target_base=="sel"', {
        'sources': [
          'dyn_array.c',
          'env_cleanser.c',
          'nacl_all_modules.c',
          'nacl_app_thread.c',
          'nacl_bottom_half.c',
          'nacl_closure.c',
          'nacl_debug.cc',
          'nacl_desc_effector_ldr.c',
          'nacl_globals.c',
          'nacl_memory_object.c',
          'nacl_signal_common.c',
          'nacl_sync_queue.c',
          'nacl_syscall_common.c',
          'nacl_syscall_hook.c',
          'nacl_text.c',
          'sel_addrspace.c',
          'sel_ldr.c',
          'sel_ldr-inl.c',
          'sel_ldr_standard.c',
          'elf_util.c',
          'sel_main_chrome.c',
          'sel_mem.c',
          'sel_qualify.c',
          'sel_util.c',
          'sel_util-inl.c',
          'sel_validate_image.c',
        ],
        'include_dirs': [
          # For generated header files from the x86-64 validator,
          # e.g. nacl_disallows.h.
          '<(SHARED_INTERMEDIATE_DIR)',
          '<(DEPTH)/gdb_utils/src',
        ],
        'sources!': [
           '<(syscall_handler)',
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
        'conditions': [
            ['OS=="mac"', {
              'sources': [
                'osx/nacl_ldt.c',
                'osx/nacl_thread_nice.c',
                'linux/sel_memory.c',
                'linux/x86/sel_segments.c',
                'osx/outer_sandbox.c',
              ],
            }],
            ['OS=="win"', {
              'sources': [
                'win/nacl_ldt.c',
                'win/nacl_thread_nice.c',
                'win/sel_memory.c',
                'win/sel_segments.c',
              ],
            }],
            # TODO(gregoryd): move arm-specific stuff into a separate gyp file.
            ['target_arch=="arm"', {
              'sources': [
                'arch/arm/nacl_app.c',
                'arch/arm/nacl_switch_to_app_arm.c',
                'arch/arm/sel_rt.c',
                'arch/arm/nacl_tls.c',
                'arch/arm/sel_ldr_arm.c',
                'arch/arm/sel_addrspace_arm.c',
                'arch/arm/nacl_switch.S',
                'arch/arm/nacl_syscall.S',
                'arch/arm/springboard.S',
                'arch/arm/tramp_arm.S',
                'linux/nacl_signal_arm.c',
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
            ['OS=="linux" or OS=="mac" or OS=="FreeBSD"', {
              'sources': [
                'posix/nacl_signal.c',
               ],
            }],
            ['OS=="win"', {
              'sources': [
                'win/nacl_signal.c',
              ],
            }],
          ],
        }],
      ],
   },
  'targets': [
    {
      'target_name': 'sel',
      'type': 'static_library',
      'variables': {
        'target_base': 'sel',
      },
      'dependencies': [
        'gio_wrapped_desc',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/gdb_rsp/gdb_rsp.gyp:gdb_rsp',
        '<(DEPTH)/native_client/src/trusted/debug_stub/debug_stub.gyp:debug_stub',
      ],
      'conditions': [
        ['target_arch=="arm"', {
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/validator_arm/validator_arm.gyp:ncvalidate_arm_v2',
          ],
        }],
        ['target_arch=="ia32" or target_arch=="x64"', {
          'dependencies': [
            'arch/x86/service_runtime_x86.gyp:service_runtime_x86_common',
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
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:ncvalidate_sfi',
          ],
        }],
        ['nacl_standalone==0 and OS=="win"', {
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:ldrhandle',
          ],
        }],
        ['OS=="win" and win32_breakpad==1', {
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/nacl_breakpad/nacl_breakpad.gyp:nacl_breakpad',
          ],
        }],
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
    }, {
      'target_name': 'gio_wrapped_desc',
      'type': 'static_library',
      'sources': [
        'gio_shm.c',
        'gio_shm_unbounded.c',
        'gio_nacl_desc.c',
      ],
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
      ],
    },
    {
      'target_name': 'sel_ldr',
      'type': 'executable',
      # TODO(gregoryd): currently building sel_ldr without SDL
      'dependencies': [
        'expiration',
        'sel',
        'gio_wrapped_desc',
        '<(DEPTH)/native_client/src/trusted/perf_counter/perf_counter.gyp:*',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
      ],
      'sources': [
        'sel_main.c',
      ],
    },
    # no tests are built here; see service_runtime_test.gyp
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'sel64',
          'type': 'static_library',
          'variables': {
            'target_base': 'sel',
            'win_target': 'x64',
          },
          'dependencies': [
            'gio_wrapped_desc64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:ncvalidate_sfi64',
            '<(DEPTH)/native_client/src/trusted/validator_x86/validator_x86.gyp:ncopcode_utils_gen',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            'arch/x86/service_runtime_x86.gyp:service_runtime_x86_common64',
            'arch/x86_64/service_runtime_x86_64.gyp:service_runtime_x86_64',
            '<(DEPTH)/native_client/src/trusted/gdb_rsp/gdb_rsp.gyp:gdb_rsp64',
            '<(DEPTH)/native_client/src/trusted/debug_stub/debug_stub.gyp:debug_stub64',
          ],
          'conditions': [
            ['nacl_standalone==0 and OS=="win"', {
              'dependencies': [
                '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:ldrhandle64',
              ],
            }],
            ['win64_breakpad==1', {
              'dependencies': [
                '<(DEPTH)/native_client/src/trusted/nacl_breakpad/nacl_breakpad.gyp:nacl_breakpad64',
              ],
            }],
          ],
        }, {
          'target_name': 'container64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'generic_container/container.c',
          ],
        }, {
          'target_name': 'expiration64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'expiration.c',
          ],
        }, {
          'target_name': 'nacl_xdr64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'fs/xdr.c',
            'fs/obj_proxy.c',
          ],
        }, {
          'target_name': 'gio_wrapped_desc64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'gio_shm.c',
            'gio_shm_unbounded.c',
            'gio_nacl_desc.c',
          ],
          'dependencies': [
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
          ],
        },
        {
          'target_name': 'sel_ldr64',
          'type': 'executable',
          'variables': {
            'win_target': 'x64',
          },
          # TODO(gregoryd): currently building sel_ldr without SDL
          'dependencies': [
            'expiration64',
            'sel64',
            'gio_wrapped_desc64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib64',
          ],
          'sources': [
            'sel_main.c',
          ],
        },
        # TODO(bsy): no tests are built; see build.scons
      ],
    }],
  ]
}
