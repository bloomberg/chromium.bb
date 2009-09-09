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
        '../../third_party/npapi',
      ],
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

    {
      'target_name': 'gpu_plugin',
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
        'command_buffer.cc',
        'command_buffer.h',
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
      'sources': [
        'command_buffer_mock.h',
        'command_buffer_unittest.cc',
        'gpu_plugin_unittest.cc',
        'gpu_plugin_object_unittest.cc',
        'gpu_plugin_object_factory_unittest.cc',
      ],
    },
  ]
}
