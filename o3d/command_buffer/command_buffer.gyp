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
        'common/cross/bitfield_helpers.h',
        'common/cross/cmd_buffer_common.h',
        'common/cross/cmd_buffer_common.cc',
        'common/cross/o3d_cmd_format.h',
        'common/cross/o3d_cmd_format.cc',
        'common/cross/gapi_interface.h',
        'common/cross/logging.h',
        'common/cross/mocks.h',
        'common/cross/resource.cc',
        'common/cross/resource.h',
        'common/cross/types.h',
      ],
    },
    {
      'target_name': 'command_buffer_common_test',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'common/cross/bitfield_helpers_test.cc',
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
        'client/cross/cmd_buffer_helper.cc',
        'client/cross/cmd_buffer_helper.h',
        'client/cross/effect_helper.cc',
        'client/cross/effect_helper.h',
        'client/cross/fenced_allocator.cc',
        'client/cross/fenced_allocator.h',
        'client/cross/id_allocator.cc',
        'client/cross/id_allocator.h',
        'client/cross/o3d_cmd_helper.cc',
        'client/cross/o3d_cmd_helper.h',
      ],
    },
    {
      'target_name': 'command_buffer_client_test',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'client/cross/cmd_buffer_helper_test.cc',
          'client/cross/fenced_allocator_test.cc',
          'client/cross/id_allocator_test.cc',
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
        'service/cross/common_decoder.cc',
        'service/cross/common_decoder.h',
        'service/cross/cmd_buffer_engine.h',
        'service/cross/cmd_parser.cc',
        'service/cross/cmd_parser.h',
        'service/cross/effect_utils.cc',
        'service/cross/effect_utils.h',
        'service/cross/gapi_decoder.cc',
        'service/cross/gapi_decoder.h',
        'service/cross/mocks.h',
        'service/cross/precompile.cc',
        'service/cross/precompile.h',
        'service/cross/resource.cc',
        'service/cross/resource.h',
        'service/cross/texture_utils.cc',
        'service/cross/texture_utils.h',
      ],
      'conditions': [
        ['OS == "win"',
          {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'ForcedIncludeFiles':
                'command_buffer/service/cross/precompile.h',
              },
            },
          },
        ],
        ['OS == "mac"',
          {
            'xcode_settings': {
              'GCC_PREFIX_HEADER': 'service/cross/precompile.h',
            },
          },
        ],
        ['OS == "linux"',
          {
            'cflags': [
              '-include',
              'command_buffer/service/cross/precompile.h',
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
              'service/win/d3d9/d3d9_utils.h',
              'service/win/d3d9/effect_d3d9.cc',
              'service/win/d3d9/effect_d3d9.h',
              'service/win/d3d9/gapi_d3d9.cc',
              'service/win/d3d9/gapi_d3d9.h',
              'service/win/d3d9/geometry_d3d9.cc',
              'service/win/d3d9/geometry_d3d9.h',
              'service/win/d3d9/render_surface_d3d9.cc',
              'service/win/d3d9/render_surface_d3d9.h',
              'service/win/d3d9/sampler_d3d9.cc',
              'service/win/d3d9/sampler_d3d9.h',
              'service/win/d3d9/states_d3d9.cc',
              'service/win/d3d9/texture_d3d9.cc',
              'service/win/d3d9/texture_d3d9.h',
            ],  # 'sources'
          },
        ],
        ['cb_service == "gl"',
          {
            'include_dirs': [
              '../../<(glewdir)/include',
              '../../<(cgdir)/include',
            ],
            'sources': [
              'service/cross/gl/effect_gl.cc',
              'service/cross/gl/effect_gl.h',
              'service/cross/gl/gapi_gl.cc',
              'service/cross/gl/gapi_gl.h',
              'service/cross/gl/geometry_gl.cc',
              'service/cross/gl/geometry_gl.h',
              'service/cross/gl/gl_utils.h',
              'service/cross/gl/render_surface_gl.cc',
              'service/cross/gl/render_surface_gl.h',
              'service/cross/gl/sampler_gl.cc',
              'service/cross/gl/sampler_gl.h',
              'service/cross/gl/states_gl.cc',
              'service/cross/gl/texture_gl.cc',
              'service/cross/gl/texture_gl.h',
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
          'service/cross/cmd_parser_test.cc',
          'service/cross/resource_test.cc',
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
