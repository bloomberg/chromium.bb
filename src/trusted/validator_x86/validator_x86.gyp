# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(bradchen): eliminate need for the warning flag removals below
{
  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'validate_gen_out':
       '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86',
  },
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="ncvalidate"', {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
        'sources': [
          'halt_trim.c',
          'nacl_cpuid.c',
          'ncdecode.c',
          'ncinstbuffer.c',
          'nc_segment.c',
          'nc_inst_iter.c',
          'nc_inst_state.c',
          'nc_inst_trans.c',
          'ncop_exps.c',
          'ncvalidate.c',
          'nccopycode.c',
          'nccopycode_stores.S',
        ],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
            '-Wswitch-enum',
            '-Wsign-compare'
          ],
        },
        # When ncvalidate is a dependency, it needs to be a hard dependency
        # because dependents may rely on ncvalidate to create header files
        # below.
      }],
      ['target_base=="ncopcode_utils"', {
        'sources': ['ncopcode_desc.c'],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
            '-Wswitch-enum',
            '-Wsign-compare'
          ]
        },
      }],
      ['target_base=="ncvalidate_sfi"', {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
        # we depend on ncvalidate build to generate the headers
        'sources': [ 'ncvalidate_iter.c',
                     'ncvalidator_registry.c',
                     'nc_opcode_histogram.c',
                     'nc_cpu_checks.c',
                     'nc_illegal.c',
                     'nc_protect_base.c',
                     'nc_memory_protect.c',
                     'ncvalidate_utils.c',
                     'nc_jumps.c',
         ],
        'cflags!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-Wextra',
            '-Wswitch-enum',
            '-Wsign-compare'
          ]
        },
      }],
      ['target_base=="ncvalidate_gen"', {
        'actions': [
          {
            'action_name': 'ncdecode_table',
            'msvs_cygwin_shell': 0,
            'inputs': [
              '<(PRODUCT_DIR)/ncdecode_table<(EXECUTABLE_SUFFIX)',
            ],
            'outputs': [
              # TODO(gregoryd): keeping the long include path for now to be
              # compatible with the scons build.
              '<(validate_gen_out)/ncdecodetab.h',
              '<(validate_gen_out)/ncdisasmtab.h',
              '<(validate_gen_out)/ncbadprefixmask.h',
            ],
            'action': ['<(PRODUCT_DIR)/ncdecode_table<(EXECUTABLE_SUFFIX)',
                       '-m32', '<@(_outputs)'],
            'message': 'Running ncdecode_table',
            'process_outputs_as_sources': 1,
          },
          {
            'action_name': 'ncdecode_tablegen_32',
            'msvs_cygwin_shell': 0,
            'inputs': [
              '<(PRODUCT_DIR)/ncdecode_tablegen<(EXECUTABLE_SUFFIX)',
            ],
            'outputs': [
              '<(validate_gen_out)/nc_opcode_table_32.h',
            ],
            'message': 'Running ncdecode_tablegen -m32',
            'process_outputs_as_sources': 1,
            'action': ['<@(_inputs)', '-m32', '<@(_outputs)'],
          },
          {
            'action_name': 'ncdecode_tablegen_64',
            'msvs_cygwin_shell': 0,
            'inputs': [
              '<(PRODUCT_DIR)/ncdecode_tablegen<(EXECUTABLE_SUFFIX)',
            ],
            'outputs': [
              '<(validate_gen_out)/nc_opcode_table_64.h',
            ],
            'message': 'Running ncdecode_tablegen -m64',
            'process_outputs_as_sources': 1,
            'action': ['<@(_inputs)', '-m64', '<@(_outputs)'],
          },
          {
            'action_name': 'ncdecode_tablegen_subregs_32',
            'msvs_cygwin_shell': 0,
            'inputs': [
              '<(PRODUCT_DIR)/ncdecode_tablegen<(EXECUTABLE_SUFFIX)',
            ],
            'outputs': [
              '<(validate_gen_out)/nc_subregs_32.h',
            ],
            'message': 'Running ncdecode_tablegen -m32 -naclsubregs',
            'process_outputs_as_sources': 1,
            'action': ['<@(_inputs)', '-m32', '-nacl_subregs', '<@(_outputs)'],
          },
          {
            'action_name': 'ncdecode_tablegen_subregs_64',
            'msvs_cygwin_shell': 0,
            'inputs': [
              '<(PRODUCT_DIR)/ncdecode_tablegen<(EXECUTABLE_SUFFIX)',
            ],
            'outputs': [
              '<(validate_gen_out)/nc_subregs_64.h',
            ],
            'message': 'Running ncdecode_tablegen -m64 -naclsubregs',
            'process_outputs_as_sources': 1,
            'action': ['<@(_inputs)', '-m64', '-nacl_subregs', '<@(_outputs)'],
          },
          {
            'action_name': 'ncop_expr_node_flag',
            'msvs_cygwin_shell': 0,
            'inputs': [ 'enum_gen.py', 'ncop_expr_node_flag.enum' ],
            'outputs': [
              '<(validate_gen_out)/ncop_expr_node_flag.h',
              '<(validate_gen_out)/ncop_expr_node_flag_impl.h',
            ],
            'action':
              ['<@(python_exe)', 'enum_gen.py',
               '--header=<(validate_gen_out)/ncop_expr_node_flag.h',
               '--source=<(validate_gen_out)/ncop_expr_node_flag_impl.h',
               '--path_prefix=<(SHARED_INTERMEDIATE_DIR)',
               '--name=NaClExpFlag',
               '--add_size=1',
               'ncop_expr_node_flag.enum'],
            'process_outputs_as_sources': 1,
            'message': 'Creating ncop_expr_node_flag.h',
          },
          {
            'action_name': 'ncop_expr_node_kind',
            'msvs_cygwin_shell': 0,
            'inputs': [ 'enum_gen.py', 'ncop_expr_node_kind.enum' ],
            'outputs': [
              '<(validate_gen_out)/ncop_expr_node_kind.h',
              '<(validate_gen_out)/ncop_expr_node_kind_impl.h',
            ],
            'action':
              ['<@(python_exe)', 'enum_gen.py',
               '--header=<(validate_gen_out)/ncop_expr_node_kind.h',
               '--source=<(validate_gen_out)/ncop_expr_node_kind_impl.h',
               '--path_prefix=<(SHARED_INTERMEDIATE_DIR)',
               '--name=NaClExpKind',
               '--add_size=1',
               'ncop_expr_node_kind.enum'],
            'process_outputs_as_sources': 1,
            'message': 'Creating ncop_expr_node_kind.h',
          },
        ],
      }],
    ],
  },
  'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncdecode_table',
      'type': 'executable',
      'sources': ['ncdecode_table.c'],
      'cflags!': [
        '-Wextra',
        '-Wswitch-enum',
        '-Wsign-compare',
      ],
      'defines': [ 'NACL_TRUSTED_BUT_NOT_TCB' ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare',
        ]
      },
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'nchelper',
      'type': 'static_library',
      'sources': [ 'ncfileutil.c' ],
      'cflags!': [
        '-Wextra',
        '-Wswitch-enum',
        '-Wsign-compare'
      ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ]
      },
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncopcode_utils',
      'type': 'static_library',
      'hard_dependency': 1,
      'variables': {
        'target_base': 'ncopcode_utils',
      },
      'dependencies': ['ncopcode_utils_gen'],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncopcode_utils_gen',
      'type': 'none',
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
      'actions': [
       {
          'action_name': 'nacl_disallows',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'nacl_disallows.enum' ],
          'outputs': [
            '<(validate_gen_out)/nacl_disallows.h',
            '<(validate_gen_out)/nacl_disallows_impl.h',
          ],
          'action':
          ['<@(python_exe)', 'enum_gen.py',
           '--header=<(validate_gen_out)/nacl_disallows.h',
           '--source=<(validate_gen_out)/nacl_disallows_impl.h',
           '--path_prefix=<(SHARED_INTERMEDIATE_DIR)',
           '--name=NaClDisallowsFlag',
           '--add_size=1',
           'nacl_disallows.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating nacl_allows.h',
       },
       {
          'action_name': 'ncopcode_prefix',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_prefix.enum' ],
          'outputs': [
            '<(validate_gen_out)/ncopcode_prefix.h',
            '<(validate_gen_out)/ncopcode_prefix_impl.h',
          ],
          'action':
          ['<@(python_exe)', 'enum_gen.py',
           '--header=<(validate_gen_out)/ncopcode_prefix.h',
           '--source=<(validate_gen_out)/ncopcode_prefix_impl.h',
           '--path_prefix=<(SHARED_INTERMEDIATE_DIR)',
           '--name=NaClInstPrefix',
           '--add_size=1',
           'ncopcode_prefix.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_prefix.h',
        },
        {
          'action_name': 'ncopcode_insts',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_insts.enum' ],
          'outputs': [
            '<(validate_gen_out)/ncopcode_insts.h',
            '<(validate_gen_out)/ncopcode_insts_impl.h',
          ],
          'action':
          ['<@(python_exe)', 'enum_gen.py',
           '--header=<(validate_gen_out)/ncopcode_insts.h',
           '--source=<(validate_gen_out)/ncopcode_insts_impl.h',
           '--path_prefix=<(SHARED_INTERMEDIATE_DIR)',
           '--name=NaClMnemonic',
           '--add_size=1',
           '--sort=1',
           '--name_prefix=Inst',
           'ncopcode_insts.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_insts.h',
        },
       {
          'action_name': 'ncopcode_opcode_flags',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_opcode_flags.enum' ],
          'outputs': [
            '<(validate_gen_out)/ncopcode_opcode_flags.h',
            '<(validate_gen_out)/ncopcode_opcode_flags_impl.h',
          ],
          'action':
          ['<@(python_exe)', 'enum_gen.py',
           '--header=<(validate_gen_out)/ncopcode_opcode_flags.h',
           '--source=<(validate_gen_out)/ncopcode_opcode_flags_impl.h',
           '--path_prefix=<(SHARED_INTERMEDIATE_DIR)',
           '--name=NaClIFlag',
           '--add_size=1',
           'ncopcode_opcode_flags.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_opcode_flags.h',
        },
        {
          'action_name': 'ncopcode_operand_kind',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_operand_kind.enum' ],
          'outputs': [
            '<(validate_gen_out)/ncopcode_operand_kind.h',
            '<(validate_gen_out)/ncopcode_operand_kind_impl.h',
          ],
          'action':
          ['<@(python_exe)', 'enum_gen.py',
           '--header=<(validate_gen_out)/ncopcode_operand_kind.h',
           '--source=<(validate_gen_out)/ncopcode_operand_kind_impl.h',
           '--path_prefix=<(SHARED_INTERMEDIATE_DIR)',
           '--name=NaClOpKind',
           '--add_size=1',
           'ncopcode_operand_kind.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_operand_kind.h',
        },
        {
          'action_name': 'ncopcode_operand_flag',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_operand_flag.enum' ],
          'outputs': [
            '<(validate_gen_out)/ncopcode_operand_flag.h',
            '<(validate_gen_out)/ncopcode_operand_flag_impl.h',
          ],
          'action':
          ['<@(python_exe)', 'enum_gen.py',
           '--header=<(validate_gen_out)/ncopcode_operand_flag.h',
           '--source=<(validate_gen_out)/ncopcode_operand_flag_impl.h',
           '--path_prefix=<(SHARED_INTERMEDIATE_DIR)',
           '--name=NaClOpFlag',
           '--add_size=1',
           'ncopcode_operand_flag.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_operand_flag.h',
        },
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncdecode_tablegen',
      'type': 'executable',
      'sources': ['ncdecode_tablegen.c',
                  'ncdecode_forms.c',
                  'zero_extends.c',
                  'long_mode.c',
                  'nc_rep_prefix.c',
                  'defsize64.c',
                  'nacl_illegal.c',
                  'lock_insts.c',
                  'ncdecode_st.c',
                  'ncdecode_onebyte.c',
                  'ncdecode_OF.c',
                  'ncdecode_sse.c',
                  'ncdecodeX87.c',
                  'nacl_regsgen.c',
                  'force_cpp.cc'
      ],
      'cflags!': [
        '-Wextra',
        '-Wswitch-enum',
        '-Wsign-compare'
      ],
      'defines': [ 'NACL_TRUSTED_BUT_NOT_TCB' ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ]
      },
      'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      'dependencies': [
        'ncopcode_utils',
        '<(DEPTH)/native_client/src/shared/utils/utils.gyp:utils',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio'
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalLibraryDirectories': [
                '$(OutDir)/lib',
              ],
            },
          },
        }],
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncvalidate_gen',
      'hard_dependency': 1,
      'type': 'none',
      'variables': {
        'target_base': 'ncvalidate_gen',
      },
      'dependencies': [
        'ncdecode_table',
        'ncdecode_tablegen',
        'ncopcode_utils',
      ],
    },
    {
      'target_name': 'ncvalidate',
      'type': 'static_library',
      'variables': {
        'target_base': 'ncvalidate',
      },
      'dependencies': [
        'ncvalidate_gen',
      ],
      'hard_dependency': 1,
    },
    # ----------------------------------------------------------------------
    { 'target_name': 'ncvalidate_sfi',
      'type': 'static_library',
      'dependencies': ['ncvalidate' ],
      'variables': {
        'target_base': 'ncvalidate_sfi',
      },
      'hard_dependency': 1,
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncdis_util',
      'type': 'static_library',
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      # we depend on ncvalidate build to generate the headers
      'dependencies': ['ncvalidate_gen' ],
      'sources': [ 'ncdis_util.c',
                   'ncdis_segments.c',
                   'nc_read_segment.c',
                   'ncenuminsts.c'
                 ],
      'cflags!': [
        '-Wextra',
        '-Wswitch-enum',
        '-Wsign-compare'
      ],
      'defines': [ 'NACL_TRUSTED_BUT_NOT_TCB' ],
      'xcode_settings': {
        'WARNING_CFLAGS!': [
          '-Wextra',
          '-Wswitch-enum',
          '-Wsign-compare'
        ]
      },
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncdis',
      'type': 'executable',
      'sources': [
        'ncdis.c',
        'force_cpp.cc'
      ],
      'defines': [ 'NACL_TRUSTED_BUT_NOT_TCB' ],
      'dependencies': [
        'ncdis_util',
        'nchelper',
        'ncopcode_utils',
        'ncopcode_utils_gen',
        'ncvalidate',
        '<(DEPTH)/native_client/src/shared/utils/utils.gyp:utils',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio'
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'ncvalidate_sfi64',
          'type': 'static_library',
          'dependencies': ['ncvalidate64' ],
          'variables': {
            'target_base': 'ncvalidate_sfi',
            'win_target': 'x64',
          },
          'hard_dependency': 1,
        },
        # ----------------------------------------------------------------------
        {
          'target_name': 'ncopcode_utils64',
          'type': 'static_library',
          'hard_dependency': 1,
          'variables': {
            'target_base': 'ncopcode_utils',
            'win_target': 'x64',
          },
          'dependencies': ['ncopcode_utils_gen'],
        },
        {
          'target_name': 'ncvalidate64',
          'type': 'static_library',
          'variables': {
            'target_base': 'ncvalidate',
            'win_target': 'x64',
          },
          'dependencies': [
            'ncvalidate_gen',
            'ncopcode_utils64',
          ],
          'hard_dependency': 1,
        },
      ],
    }],
  ],
}
