# -*- python -*-
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
      ['target_base=="sel"', {
        'sources': [
          'dyn_array.c',
          'elf_util.c',
          'nacl_all_modules.c',
          'nacl_app_thread.c',
          'nacl_desc_effector_ldr.c',
          'nacl_globals.c',
          'nacl_kern_services.c',
          'nacl_memory_object.c',
          'nacl_signal_common.c',
          'nacl_stack_safety.c',
          'nacl_syscall_common.c',
          'nacl_syscall_hook.c',
          'nacl_text.c',
          'nacl_valgrind_hooks.c',
          'name_service/default_name_service.c',
          'name_service/name_service.c',
          'sel_addrspace.c',
          'sel_ldr.c',
          'sel_ldr-inl.c',
          'sel_ldr_standard.c',
          'sel_ldr_thread_interface.c',
          'sel_main_chrome.c',
          'sel_mem.c',
          'sel_qualify.c',
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
                'osx/nacl_oop_debugger_hooks.c',
                'osx/nacl_thread_nice.c',
                'linux/sel_memory.c',
                'linux/x86/sel_segments.c',
                'osx/outer_sandbox.c',
              ],
            }],
            ['OS=="win"', {
              'sources': [
                'win/nacl_ldt.c',
                'win/nacl_oop_debugger_hooks.c',
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
                'linux/nacl_oop_debugger_hooks.c',
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
        'env_cleanser',
        'nacl_error_code',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc',
        '<(DEPTH)/native_client/src/trusted/debug_stub/debug_stub.gyp:debug_stub',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/gdb_rsp/gdb_rsp.gyp:gdb_rsp',
        '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc',
        '<(DEPTH)/native_client/src/trusted/perf_counter/perf_counter.gyp:nacl_perf_counter',
        '<(DEPTH)/native_client/src/trusted/manifest_name_service_proxy/manifest_name_service_proxy.gyp:manifest_proxy',
        '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service',
        '<(DEPTH)/native_client/src/trusted/threading/threading.gyp:thread_interface',
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
            '<(DEPTH)/native_client/src/trusted/validator/x86/32/validator_x86_32.gyp:ncvalidate_x86_32',
          ],
        }],
        ['target_arch == "x64"', {
          'dependencies': [
            'arch/x86_64/service_runtime_x86_64.gyp:service_runtime_x86_64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/64/validator_x86_64.gyp:ncvalidate_x86_64',
          ],
        }],
        ['nacl_standalone==0 and OS=="win"', {
          'dependencies': [
            '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:handle_lookup',
            '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:browserhandle',
            '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:ldrhandle',
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
      'target_name': 'nacl_xdr',
      'type': 'static_library',
      'sources': [
        'fs/xdr.c',
        'fs/obj_proxy.c',
      ],
    }, {
      'target_name': 'nacl_error_code',
      'type': 'static_library',
      'sources': [
        'nacl_error_code.c',
      ],
    }, {
      'target_name': 'env_cleanser',
      'type': 'static_library',
      'sources': [
        'env_cleanser.c',
      ],
    }, {
      'target_name': 'sel_ldr',
      'type': 'executable',
      'dependencies': [
        'sel',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc',
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
            'env_cleanser64',
            'nacl_error_code64',
            '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio64',
            '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:nonnacl_srpc64',
            '<(DEPTH)/native_client/src/trusted/debug_stub/debug_stub.gyp:debug_stub64',
            '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer64',
            '<(DEPTH)/native_client/src/trusted/gdb_rsp/gdb_rsp.gyp:gdb_rsp64',
            '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc64',
            '<(DEPTH)/native_client/src/trusted/perf_counter/perf_counter.gyp:nacl_perf_counter64',
            '<(DEPTH)/native_client/src/trusted/manifest_name_service_proxy/manifest_name_service_proxy.gyp:manifest_proxy64',
            '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service64',
            '<(DEPTH)/native_client/src/trusted/threading/threading.gyp:thread_interface64',
            '<(DEPTH)/native_client/src/trusted/validator/x86/64/validator_x86_64.gyp:ncvalidate_x86_64',
            'arch/x86/service_runtime_x86.gyp:service_runtime_x86_common64',
            'arch/x86_64/service_runtime_x86_64.gyp:service_runtime_x86_64',
          ],
          'conditions': [
            ['nacl_standalone==0 and OS=="win"', {
              'dependencies': [
                '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:handle_lookup64',
                '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:browserhandle64',
                '<(DEPTH)/native_client/src/trusted/handle_pass/handle_pass.gyp:ldrhandle64',
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
          'target_name': 'nacl_xdr64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'fs/xdr.c',
            'fs/obj_proxy.c',
          ],
        },
        {
          'target_name': 'nacl_error_code64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'nacl_error_code.c',
          ],
        },
        {
          'target_name': 'env_cleanser64',
          'type': 'static_library',
          'variables': {
            'win_target': 'x64',
          },
          'sources': [
            'env_cleanser.c',
          ],
        },
        {
          'target_name': 'sel_ldr64',
          'type': 'executable',
          'variables': {
            'win_target': 'x64',
          },
          'dependencies': [
            'sel64',
            '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform64',
            '<(DEPTH)/native_client/src/trusted/gio/gio_wrapped_desc.gyp:gio_wrapped_desc64',
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
