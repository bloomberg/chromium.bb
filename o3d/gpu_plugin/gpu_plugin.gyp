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
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '../..',
        ],
      },  # 'all_dependent_settings'
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
        'np_utils/np_plugin_object_mock.h',
        'np_utils/np_plugin_object_factory.cc',
        'np_utils/np_plugin_object_factory.h',
        'np_utils/np_plugin_object_factory_mock.h',
        'np_utils/np_utils.cc',
        'np_utils/np_utils.h',
        'np_utils/webkit_browser.h',
        'system_services/shared_memory.cc',
        'system_services/shared_memory.h',
        'system_services/shared_memory_mock.h',
      ],
    },

    # This is a standalone executable until O3D is fully moved over to using
    # gyp. At that point these can become part of the regular O3D unit tests.
    {
      'target_name': 'np_utils_unittests',
      'type': 'executable',
      'dependencies': [
        'np_utils',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gmock.gyp:gmockmain',
        '../../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'np_utils/dispatched_np_object_unittest.cc',
        'np_utils/dynamic_np_object_unittest.cc',
        'np_utils/np_class_unittest.cc',
        'np_utils/np_object_pointer_unittest.cc',
        'np_utils/np_utils_unittest.cc',
        'system_services/shared_memory_unittest.cc',
      ],
    },

    # These can eventually be merged back into the gpu_plugin target. There
    # separated for now so O3D can statically link against them and use command
    # buffers in-process without the GPU plugin.
    {
      'target_name': 'command_buffer',
      'type': '<(library)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../command_buffer/command_buffer.gyp:command_buffer_service',
        'np_utils',
      ],
      'include_dirs': [
        '../..',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '../..',
        ],
      },  # 'all_dependent_settings'
      'sources': [
        'command_buffer.cc',
        'command_buffer.h',
        'command_buffer_mock.h',
        'gpu_processor.h',
        'gpu_processor.cc',
        'gpu_processor_win.cc',
      ],
    },

    {
      'target_name': 'gpu_plugin',
      'type': '<(library)',
      'dependencies': [
        '../../base/base.gyp:base',
        'command_buffer',
        'np_utils',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'gpu_plugin.cc',
        'gpu_plugin.h',
        'gpu_plugin_object.cc',
        'gpu_plugin_object.h',
        'gpu_plugin_object_win.cc',
        'gpu_plugin_object_factory.cc',
        'gpu_plugin_object_factory.h',
      ],
    },

    # This is a standalone executable until O3D is fully moved over to using
    # gyp. At that point these can become part of the regular O3D unit tests.
    {
      'target_name': 'gpu_plugin_unittests',
      'type': 'executable',
      'dependencies': [
        '../command_buffer/command_buffer.gyp:command_buffer_service',
        'gpu_plugin',
        'np_utils',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gmock.gyp:gmockmain',
        '../../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../..',
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

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
