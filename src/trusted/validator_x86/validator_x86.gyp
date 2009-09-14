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
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ncdecode_table',
      'type': 'executable',
      'sources': ['ncdecode_table.c'],
    },
    {
      'target_name': 'nchelper',
      'type': 'static_library',
      'sources': [ 'ncfileutil.c' ]
    },
    {
      'target_name': 'ncopcode_utils',
      'type': 'static_library',
      'sources': [ 'ncopcode_desc.c' ],
      'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      'actions': [
       {
          'action_name': 'ncopcode_prefix',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_prefix.enum' ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_prefix.h',
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_prefix_impl.h',
          ],
          'action':
          ['python', 'enum_gen.py',
           "--header='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_prefix.h'",
           "--source='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_prefix_impl.h'",
           "--path_prefix='<(SHARED_INTERMEDIATE_DIR)'",
           "--name='OpcodePrefix'",
           "--add_size=1",
           'ncopcode_prefix.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_prefix.h',
        },
        {
          'action_name': 'ncopcode_insts',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_insts.enum' ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_insts.h',
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_insts_impl.h',
          ],
          'action':
          ['python', 'enum_gen.py',
           "--header='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_insts.h'",
           "--source='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_insts_impl.h'",
           "--path_prefix='<(SHARED_INTERMEDIATE_DIR)'",
           "--name='InstMnemonic'",
           "--add_size=1",
           "--sort=1",
           "--name_prefix='Inst'",
           'ncopcode_insts.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_insts.h',
        },
       {
          'action_name': 'ncopcode_opcode_flags',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_opcode_flags.enum' ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_opcode_flags.h',
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_opcode_flags_impl.h',
          ],
          'action':
          ['python', 'enum_gen.py',
           "--header='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_opcode_flags.h'",
           "--source='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_opcode_flags_impl.h'",
           "--path_prefix='<(SHARED_INTERMEDIATE_DIR)'",
           "--name='OpcodeFlag'",
           "--add_size=1",
           'ncopcode_opcode_flags.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_opcode_flags.h',
        },
       {
          'action_name': 'ncopcode_operand_kind',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_operand_kind.enum' ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_operand_kind.h',
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_operand_kind_impl.h',
          ],
          'action':
          ['python', 'enum_gen.py',
           "--header='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_operand_kind.h'",
           "--source='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_operand_kind_impl.h'",
           "--path_prefix='<(SHARED_INTERMEDIATE_DIR)'",
           "--name='OperandKind'",
           "--add_size=1",
           'ncopcode_operand_kind.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_operand_kind.h',
        },
       {
          'action_name': 'ncopcode_operand_flag',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncopcode_operand_flag.enum' ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_operand_flag.h',
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_operand_flag_impl.h',
          ],
          'action':
          ['python', 'enum_gen.py',
           "--header='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_operand_flag.h'",
           "--source='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncopcode_operand_flag_impl.h'",
           "--path_prefix='<(SHARED_INTERMEDIATE_DIR)'",
           "--name='OperandFlag'",
           "--add_size=1",
           'ncopcode_operand_flag.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncopcode_operand_flag.h',
        },
      ],
    },
    {
      'target_name': 'ncdecode_tablegen',
      'type': 'executable',
      'sources': ['ncdecode_tablegen.c',
                  'ncdecode_onebyte.c',
                  'ncdecode_OF.c',
                  'ncdecode_DC.c',
                  'ncdecode_sse.c'
      ],
      'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      'dependencies': ['ncopcode_utils' ],
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
    {
      'target_name': 'ncvalidate',
      'type': 'static_library',
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'dependencies': [
        'ncdecode_table',
        'ncdecode_tablegen',
      ],
      'sources': [
        'nacl_cpuid.c',
        'ncdecode.c',
        'nc_segment.c',
        'nc_inst_iter.c',
        'nc_inst_state.c',
        'nc_inst_trans.c',
        'ncop_exps.c',
        'nc_read_segment.c',
        'ncvalidate.c',
      ],
      'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      'actions': [
        {
          'action_name': 'ncdecode_table',
          'msvs_cygwin_shell': 0,
          'inputs': [
            '<(PRODUCT_DIR)/ncdecode_table',
          ],
          'outputs': [
            # TODO(gregoryd): keeping the long include path for now to be
            # compatible with the scons build.
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncdecodetab.h',
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncdisasmtab.h',
          ],
          'action':
             ['<(PRODUCT_DIR)/ncdecode_table', '-m32', '<@(_outputs)'],
          'message': 'Running ncdecode_table',
          'process_outputs_as_sources': 1,
        },
        {
          'action_name': 'ncdecode_tablegen',
          'msvs_cygwin_shell': 0,
          'inputs': [
            '<(PRODUCT_DIR)/ncdecode_tablegen',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/nc_opcode_table.h',
          ],
          'action':
             ['<@(_inputs)', '-m32', '<@(_outputs)'],
          'message': 'Running ncdecode_tablegen',
          'process_outputs_as_sources': 1,
        },
       {
          'action_name': 'ncop_expr_node_flag',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncop_expr_node_flag.enum' ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncop_expr_node_flag.h',
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncop_expr_node_flag_impl.h',
          ],
          'action':
          ['python', 'enum_gen.py',
           "--header='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncop_expr_node_flag.h'",
           "--source='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncop_expr_node_flag_impl.h'",
           "--path_prefix='<(SHARED_INTERMEDIATE_DIR)'",
           "--name='ExprNodeFlag'",
           "--add_size=1",
           'ncop_expr_node_flag.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncop_expr_node_flag.h',
        },
       {
          'action_name': 'ncop_expr_node_kind',
          'msvs_cygwin_shell': 0,
          'inputs': [ 'enum_gen.py', 'ncop_expr_node_kind.enum' ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncop_expr_node_kind.h',
            '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncop_expr_node_kind_impl.h',
          ],
          'action':
          ['python', 'enum_gen.py',
           "--header='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncop_expr_node_kind.h'",
           "--source='<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_x86/ncop_expr_node_kind_impl.h'",
           "--path_prefix='<(SHARED_INTERMEDIATE_DIR)'",
           "--name='ExprNodeKind'",
           "--add_size=1",
           'ncop_expr_node_kind.enum'],
          'process_outputs_as_sources': 1,
          'message': 'Creating ncop_expr_node_kind.h',
        },
      ],
    },
    { 'target_name': 'ncvalidate_sfi',
      'type': 'static_library',
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      # we depend on ncvalidate build to generate the headers
      'dependencies': ['ncvalidate' ],
      'sources': [ 'ncvalidate_iter.c',
                   'nc_opcode_histogram.c',
                   'nc_cpu_checks.c',
                   'nc_illegal.c',
                   'nc_protect_base.c',
                   'nc_store_protect.c',
                   'ncvalidate_utils.c',
                   'nc_jumps.c',
                   'ncval_driver.c'
       ],
      'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
    },
    {
      'target_name': 'ncdis_util',
      'type': 'static_library',
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      # we depend on ncvalidate build to generate the headers
      'dependencies': ['ncvalidate' ],
      'sources': [ 'ncdis_util.c' ]
    },
  ]
}
