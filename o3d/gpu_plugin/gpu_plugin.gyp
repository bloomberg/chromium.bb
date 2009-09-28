# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'np_utils',
      'type': '<(library)',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
        
        # Chrome NPAPI header dir appears before the O3D one so it takes
        # priority. TODO(apatrick): one set of NPAPI headers.
        '../../third_party/npapi/bindings',
        '../../third_party/npapi/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          # Chrome NPAPI header dir appears before the O3D one so it takes
          # priority. TODO(apatrick): one set of NPAPI headers.
          '../../third_party/npapi/bindings',
          '../../third_party/npapi/include',
        ],
      },  # 'direct_dependent_settings'
      'sources': [
        'np_utils/default_np_object.h',
        'np_utils/dynamic_np_object.cc',
        'np_utils/dynamic_np_object.h',
        'np_utils/np_browser.cc',
        'np_utils/np_browser.h',
        'np_utils/np_browser_mock.h',
        'np_utils/np_browser_stub.cc',
        'np_utils/np_browser_stub.h',
        'np_utils/np_class.h',
        'np_utils/np_dispatcher.cc',
        'np_utils/np_dispatcher.h',
        'np_utils/np_dispatcher_specializations.h',
        'np_utils/np_headers.h',
        'np_utils/np_object_mock.h',
        'np_utils/np_object_pointer.h',
        'np_utils/np_plugin_object.h',
        'np_utils/np_plugin_object_factory.cc',
        'np_utils/np_plugin_object_factory.h',
        'np_utils/np_plugin_object_factory_mock.h',
        'np_utils/np_plugin_object_mock.h',
        'np_utils/np_utils.cc',
        'np_utils/np_utils.h',
        'np_utils/webkit_browser.h',
      ],
    },

    # This is a standalone executable until O3D is fully moved over to using
    # gyp. At that point these can become part of the regular O3D unit tests.
    {
      'target_name': 'np_utils_unittests',
      'type': 'executable',
      'dependencies': [
        'gpu_plugin',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gmock.gyp:gmockmain',
        '../../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../..',
        '../../third_party/npapi',
      ],
      'sources': [
        'np_utils/dispatched_np_object_unittest.cc',
        'np_utils/dynamic_np_object_unittest.cc',
        'np_utils/np_class_unittest.cc',
        'np_utils/np_object_pointer_unittest.cc',
        'np_utils/np_utils_unittest.cc',
      ],
    },

    {
      'target_name': 'system_services',
      'type': '<(library)',
      'dependencies': [
        '../../base/base.gyp:base',
        'np_utils',
      ],
      'include_dirs': [
        '../..',
        '../../third_party/npapi',
      ],
      'sources': [
        'system_services/shared_memory.cc',
        'system_services/shared_memory.h',
        'system_services/shared_memory_mock.h',
        'system_services/shared_memory_public.h',
      ],
    },

    # This is a standalone executable until O3D is fully moved over to using
    # gyp. At that point these can become part of the regular O3D unit tests.
    {
      'target_name': 'system_services_unittests',
      'type': 'executable',
      'dependencies': [
        'system_services',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gmock.gyp:gmockmain',
        '../../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../..',
        '../../third_party/npapi',
      ],
      'sources': [
        'system_services/shared_memory_unittest.cc',
      ],
    },

    # This builds a subset of the O3D command buffer common library. This is a
    # separate library for the time being because I need a subset that is not
    # dependent on NaCl.
    {
      'target_name': 'command_buffer_common_subset',
      'type': '<(library)',
      'include_dirs': [
        '..',
        '../..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },  # 'direct_dependent_settings'
      'sources': [
        '../command_buffer/common/cross/bitfield_helpers.h',
        '../command_buffer/common/cross/cmd_buffer_format.h',
        '../command_buffer/common/cross/gapi_interface.h',
        '../command_buffer/common/cross/logging.h',
        '../command_buffer/common/cross/mocks.h',
        '../command_buffer/common/cross/resource.cc',
        '../command_buffer/common/cross/resource.h',
        '../command_buffer/common/cross/types.h',
      ],
    },

    # This builds a subset of the O3D command buffer service. This is a separate
    # library for the time being because I need a subset that is not dependent
    # on NaCl.
    {
      'target_name': 'command_buffer_service_subset',
      'type': '<(library)',
      'include_dirs': [
        '..',
        '../..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },  # 'direct_dependent_settings'
      'dependencies': [
        'command_buffer_common_subset',
      ],
      'sources': [
        '../command_buffer/service/cross/cmd_parser.cc',
        '../command_buffer/service/cross/cmd_parser.h',
        '../command_buffer/service/cross/effect_utils.cc',
        '../command_buffer/service/cross/effect_utils.h',
        '../command_buffer/service/cross/gapi_decoder.cc',
        '../command_buffer/service/cross/gapi_decoder.h',
        '../command_buffer/service/cross/mocks.h',
        '../command_buffer/service/cross/precompile.cc',
        '../command_buffer/service/cross/precompile.h',
        '../command_buffer/service/cross/resource.cc',
        '../command_buffer/service/cross/resource.h',
        '../command_buffer/service/cross/texture_utils.cc',
        '../command_buffer/service/cross/texture_utils.h',
      ],

      'conditions': [
        ['OS == "win"',
          {
            'sources': [
              '../command_buffer/service/win/d3d9/d3d9_utils.h',
              '../command_buffer/service/win/d3d9/effect_d3d9.cc',
              '../command_buffer/service/win/d3d9/effect_d3d9.h',
              '../command_buffer/service/win/d3d9/gapi_d3d9.cc',
              '../command_buffer/service/win/d3d9/gapi_d3d9.h',
              '../command_buffer/service/win/d3d9/geometry_d3d9.cc',
              '../command_buffer/service/win/d3d9/geometry_d3d9.h',
              '../command_buffer/service/win/d3d9/render_surface_d3d9.cc',
              '../command_buffer/service/win/d3d9/render_surface_d3d9.h',
              '../command_buffer/service/win/d3d9/sampler_d3d9.cc',
              '../command_buffer/service/win/d3d9/sampler_d3d9.h',
              '../command_buffer/service/win/d3d9/states_d3d9.cc',
              '../command_buffer/service/win/d3d9/texture_d3d9.cc',
              '../command_buffer/service/win/d3d9/texture_d3d9.h',
            ],  # 'sources'
            'include_dirs': [
              '$(DXSDK_DIR)/Include',
            ],
            'direct_dependent_settings': {
              'include_dirs': [
                '$(DXSDK_DIR)/Include',
              ],
            },  # 'direct_dependent_settings'
          },
        ],
        ['OS == "mac" or OS == "linux"',
          {
            'sources': [
              '../command_buffer/service/cross/gl/effect_gl.cc',
              '../command_buffer/service/cross/gl/effect_gl.h',
              '../command_buffer/service/cross/gl/gapi_gl.cc',
              '../command_buffer/service/cross/gl/gapi_gl.h',
              '../command_buffer/service/cross/gl/geometry_gl.cc',
              '../command_buffer/service/cross/gl/geometry_gl.h',
              '../command_buffer/service/cross/gl/gl_utils.h',
              '../command_buffer/service/cross/gl/sampler_gl.cc',
              '../command_buffer/service/cross/gl/sampler_gl.h',
              '../command_buffer/service/cross/gl/states_gl.cc',
              '../command_buffer/service/cross/gl/texture_gl.cc',
              '../command_buffer/service/cross/gl/texture_gl.h',
            ],  # 'sources'
          },
        ],
        ['OS == "linux"',
          {
            'sources': [
              '../command_buffer/service/linux/x_utils.cc',
              '../command_buffer/service/linux/x_utils.h',
            ],
          },
        ],
      ],
    },

    {
      'target_name': 'gpu_plugin',
      'type': '<(library)',
      'dependencies': [
        '../../base/base.gyp:base',
        'command_buffer_service_subset',
        'np_utils',
      ],
      'include_dirs': [
        '../..',
        '../../third_party/npapi',
      ],
      'sources': [
        'command_buffer.cc',
        'command_buffer.h',
        'command_buffer_mock.h',
        'gpu_plugin.cc',
        'gpu_plugin.h',
        'gpu_plugin_object.cc',
        'gpu_plugin_object.h',
        'gpu_plugin_object_win.cc',
        'gpu_plugin_object_factory.cc',
        'gpu_plugin_object_factory.h',
        'gpu_processor.h',
        'gpu_processor.cc',
        'gpu_processor_win.cc',
      ],
    },

    # This is a standalone executable until O3D is fully moved over to using
    # gyp. At that point these can become part of the regular O3D unit tests.
    {
      'target_name': 'gpu_plugin_unittests',
      'type': 'executable',
      'dependencies': [
        'command_buffer_service_subset',
        'gpu_plugin',
        'np_utils',
        'system_services',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gmock.gyp:gmockmain',
        '../../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../..',
        '../../third_party/npapi',
      ],
      'conditions': [
        ['OS == "win" and (renderer == "d3d9" or renderer == "cb")',
          {
            # These dependencies are temporary until the command buffer code
            # loads D3D and D3DX dynamically.
            'link_settings': {
              'libraries': [
                '"$(DXSDK_DIR)/Lib/x86/DxErr.lib"',
                '"$(DXSDK_DIR)/Lib/x86/d3dx9.lib"',
                '-ld3d9.lib',
              ],
            },
          },
        ],
      ],
      'sources': [
        'command_buffer_unittest.cc',
        'gpu_plugin_unittest.cc',
        'gpu_plugin_object_unittest.cc',
        'gpu_plugin_object_factory_unittest.cc',
        'gpu_processor_unittest.cc',
      ],
    },
  ]
}
