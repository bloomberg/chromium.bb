# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '..',
      '../..',
      '../../<(gtestdir)',
      '../../<(nacldir)',
    ],
    # TODO(rlp): remove this after fixing signed / unsigned issues in
    # command buffer code and tests.
    'target_conditions': [
      ['OS == "mac"',
        {
          'xcode_settings': {
            'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO'
          },
        },
      ],
    ],
  },
  'targets': [
    {
      'target_name': 'command_buffer_common',
      'type': 'static_library',
      'all_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },  # 'all_dependent_settings'
      'sources': [
        'common/bitfield_helpers.h',
        'common/cmd_buffer_common.h',
        'common/cmd_buffer_common.cc',
        'common/o3d_cmd_format.h',
        'common/o3d_cmd_format.cc',
        'common/gapi_interface.h',
        'common/logging.h',
        'common/mocks.h',
        'common/resource.cc',
        'common/resource.h',
        'common/types.h',
      ],
    },
    {
      'target_name': 'command_buffer_common_test',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'common/bitfield_helpers_test.cc',
        ],
      },
    },
    {
      'target_name': 'command_buffer_client',
      'type': 'static_library',
      'dependencies': [
        'command_buffer_common',
        '../gpu_plugin/gpu_plugin.gyp:np_utils',
      ],
      'sources': [
        'client/cmd_buffer_helper.cc',
        'client/cmd_buffer_helper.h',
        'client/effect_helper.cc',
        'client/effect_helper.h',
        'client/fenced_allocator.cc',
        'client/fenced_allocator.h',
        'client/id_allocator.cc',
        'client/id_allocator.h',
        'client/o3d_cmd_helper.cc',
        'client/o3d_cmd_helper.h',
      ],
    },
    {
      'target_name': 'command_buffer_client_test',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'client/cmd_buffer_helper_test.cc',
          'client/fenced_allocator_test.cc',
          'client/id_allocator_test.cc',
        ],
      },
    },
    {
      'target_name': 'command_buffer_service',
      'type': 'static_library',
      'all_dependent_settings': {
        'include_dirs': [
          '..',
        ],
        'conditions': [
          ['OS == "win" and (renderer == "gl" or cb_service == "gl")',
            {
              'include_dirs': [
                '../../<(glewdir)/include',
                '../../<(cgdir)/include',
              ],
            },
          ],
        ],
      },  # 'all_dependent_settings'
      'dependencies': [
        'command_buffer_common',
      ],
      'sources': [
        'service/common_decoder.cc',
        'service/common_decoder.h',
        'service/cmd_buffer_engine.h',
        'service/cmd_parser.cc',
        'service/cmd_parser.h',
        'service/effect_utils.cc',
        'service/effect_utils.h',
        'service/gapi_decoder.cc',
        'service/gapi_decoder.h',
        'service/mocks.h',
        'service/precompile.cc',
        'service/precompile.h',
        'service/resource.cc',
        'service/resource.h',
        'service/texture_utils.cc',
        'service/texture_utils.h',
      ],
      'conditions': [
        ['OS == "win"',
          {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'ForcedIncludeFiles':
                'command_buffer/service/precompile.h',
              },
            },
          },
        ],
        ['OS == "mac"',
          {
            'xcode_settings': {
              'GCC_PREFIX_HEADER': 'service/precompile.h',
            },
          },
        ],
        ['OS == "linux"',
          {
            'cflags': [
              '-include',
              'command_buffer/service/precompile.h',
            ],
          },
        ],
        ['cb_service == "d3d9"',
          {
            'include_dirs': [
              '$(DXSDK_DIR)/Include',
            ],
            'all_dependent_settings': {
              'include_dirs': [
                '$(DXSDK_DIR)/Include',
              ],
              'link_settings': {
                'libraries': [
                  '"$(DXSDK_DIR)/Lib/x86/DxErr.lib"',
                ],
              },
            },  # 'all_dependent_settings'
            'sources': [
              'service/d3d9_utils.h',
              'service/effect_d3d9.cc',
              'service/effect_d3d9.h',
              'service/gapi_d3d9.cc',
              'service/gapi_d3d9.h',
              'service/geometry_d3d9.cc',
              'service/geometry_d3d9.h',
              'service/render_surface_d3d9.cc',
              'service/render_surface_d3d9.h',
              'service/sampler_d3d9.cc',
              'service/sampler_d3d9.h',
              'service/states_d3d9.cc',
              'service/texture_d3d9.cc',
              'service/texture_d3d9.h',
            ],  # 'sources'
          },
        ],
        ['cb_service == "gl"',
          {
            'dependencies': [
              '../build/libs.gyp:gl_libs',
              '../build/libs.gyp:cg_libs',
            ],
            'sources': [
              'service/effect_gl.cc',
              'service/effect_gl.h',
              'service/gapi_gl.cc',
              'service/gapi_gl.h',
              'service/geometry_gl.cc',
              'service/geometry_gl.h',
              'service/gl_utils.h',
              'service/render_surface_gl.cc',
              'service/render_surface_gl.h',
              'service/sampler_gl.cc',
              'service/sampler_gl.h',
              'service/states_gl.cc',
              'service/texture_gl.cc',
              'service/texture_gl.h',
            ],  # 'sources'
          },
        ],
        ['OS == "linux"',
          {
            'sources': [
              'service/linux/x_utils.cc',
              'service/linux/x_utils.h',
            ],
          },
        ],
      ],  # 'conditions'
    },
    {
      'target_name': 'command_buffer_service_test',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'service/cmd_parser_test.cc',
          'service/resource_test.cc',
        ],
      },
    },
  ],  # 'targets'
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
