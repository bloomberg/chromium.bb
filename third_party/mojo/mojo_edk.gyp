# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'target_defaults' : {
    'include_dirs': [
      # TODO(use_chrome_edk): since we include a few headers from src/mojo/edk,
      # we need their includes to be searched first (i.e. otherwise when
      # embedder.cc in third_party includes core.h from src/mojo/edk, and the
      # latter includes mojo/edk/system/memory.h, the header from third_party
      # would incorrectly get chosen).
      '../..',
    ],
  },
  'targets': [
    {
      # GN version: //mojo/edk/system
      'target_name': 'mojo_system_impl',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        # TODO(use_chrome_edk): so that EDK in third_party can choose the EDK in
        # src/mojo if the command line flag is specified. It has to since we can
        # only have one definition of the Mojo primitives.
        '../../mojo/mojo_edk.gyp:mojo_system_impl2',
      ],
      'includes': [
        'mojo_edk_system_impl.gypi',
      ],
    },
    {
      # GN version: //mojo/edk/js
      'target_name': 'mojo_js_lib',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../gin/gin.gyp:gin',
        '../../v8/tools/gyp/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '../../base/base.gyp:base',
        '../../gin/gin.gyp:gin',
      ],
      'sources': [
        # Sources list duplicated in GN build.
        'src/mojo/edk/js/core.cc',
        'src/mojo/edk/js/core.h',
        'src/mojo/edk/js/drain_data.cc',
        'src/mojo/edk/js/drain_data.h',
        'src/mojo/edk/js/handle.cc',
        'src/mojo/edk/js/handle.h',
        'src/mojo/edk/js/handle_close_observer.h',
        'src/mojo/edk/js/mojo_runner_delegate.cc',
        'src/mojo/edk/js/mojo_runner_delegate.h',
        'src/mojo/edk/js/support.cc',
        'src/mojo/edk/js/support.h',
        'src/mojo/edk/js/threading.cc',
        'src/mojo/edk/js/threading.h',
        'src/mojo/edk/js/waiting_callback.cc',
        'src/mojo/edk/js/waiting_callback.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support_impl
      'target_name': 'mojo_test_support_impl',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'src/mojo/edk/test/test_support_impl.cc',
        'src/mojo/edk/test/test_support_impl.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support
      'target_name': 'mojo_common_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        'mojo_system_impl',
      ],
      'sources': [
        'src/mojo/edk/test/multiprocess_test_helper.cc',
        'src/mojo/edk/test/multiprocess_test_helper.h',
        'src/mojo/edk/test/scoped_ipc_support.cc',
        'src/mojo/edk/test/scoped_ipc_support.h',
        'src/mojo/edk/test/test_utils.h',
        'src/mojo/edk/test/test_utils_posix.cc',
        'src/mojo/edk/test/test_utils_win.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'src/mojo/edk/test/multiprocess_test_helper.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_unittests
      'target_name': 'mojo_run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        'mojo_system_impl',
        'mojo_public.gyp:mojo_public_test_support',
        'mojo_test_support_impl',
      ],
      'sources': [
        'src/mojo/edk/test/run_all_unittests.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_perftests
      'target_name': 'mojo_run_all_perftests',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:test_support_base',
        'mojo_edk.gyp:mojo_system_impl',
        'mojo_public.gyp:mojo_public_test_support',
        'mojo_test_support_impl',
      ],
      'sources': [
        'src/mojo/edk/test/run_all_perftests.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'mojo_system_impl_win64',
          'type': '<(component)',
          'dependencies': [
            '../../base/base.gyp:base_win64',
            '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
          ],
          'includes': [
            'mojo_edk_system_impl.gypi',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
