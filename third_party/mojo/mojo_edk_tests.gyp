# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      'target_name': 'mojo_edk_tests',
      'type': 'none',
      'dependencies': [
        # NOTE: If adding a new dependency here, please consider whether it
        # should also be added to the list of Mojo-related dependencies of
        # build/all.gyp:All on iOS, as All cannot depend on the mojo_base
        # target on iOS due to the presence of the js targets, which cause v8
        # to be built.
        'mojo_message_pipe_perftests',
        'mojo_public_bindings_perftests',
        'mojo_public_bindings_unittests',
        'mojo_public_environment_unittests',
        'mojo_public_system_perftests',
        'mojo_public_system_unittests',
        'mojo_public_utility_unittests',
        'mojo_system_unittests',
      ],
    },
    # TODO(vtl): Reorganize the mojo_public_*_unittests.
    {
      # GN version: //mojo/edk/test:mojo_public_bindings_unittests
      'target_name': 'mojo_public_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../../mojo/mojo_base.gyp:mojo_environment_chromium',
        '../../mojo/mojo_base.gyp:mojo_message_pump_lib',
        '../../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public.gyp:mojo_public_bindings_test_utils',
        'mojo_public.gyp:mojo_public_test_associated_interfaces',
        'mojo_public.gyp:mojo_public_test_interfaces',
        'mojo_public.gyp:mojo_public_test_interfaces_blink',
        'mojo_public.gyp:mojo_public_test_interfaces_chromium',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        '../../mojo/public/cpp/bindings/tests/array_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/associated_interface_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/binding_callback_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/binding_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/bounds_checker_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/buffer_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/callback_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/connector_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/constant_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/container_test_util.cc',
        '../../mojo/public/cpp/bindings/tests/equals_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/handle_passing_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/interface_ptr_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/map_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/message_queue.cc',
        '../../mojo/public/cpp/bindings/tests/message_queue.h',
        '../../mojo/public/cpp/bindings/tests/multiplex_router_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/pickle_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/pickled_struct_blink.cc',
        '../../mojo/public/cpp/bindings/tests/pickled_struct_blink.h',
        '../../mojo/public/cpp/bindings/tests/pickled_struct_chromium.cc',
        '../../mojo/public/cpp/bindings/tests/pickled_struct_chromium.h',
        '../../mojo/public/cpp/bindings/tests/rect_blink.h',
        '../../mojo/public/cpp/bindings/tests/rect_chromium.h',
        '../../mojo/public/cpp/bindings/tests/request_response_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/router_test_util.cc',
        '../../mojo/public/cpp/bindings/tests/router_test_util.h',
        '../../mojo/public/cpp/bindings/tests/router_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/sample_service_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/serialization_warning_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/string_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/struct_traits_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/struct_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/type_conversion_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/union_unittest.cc',
        '../../mojo/public/cpp/bindings/tests/validation_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:mojo_public_bindings_perftests
      'target_name': 'mojo_public_bindings_perftests',
      'type': 'executable',
      'dependencies': [
        '../../mojo/mojo_base.gyp:mojo_common_lib',
        '../../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public.gyp:mojo_environment_standalone',
        'mojo_public.gyp:mojo_public_bindings_test_utils',
        'mojo_public.gyp:mojo_public_test_interfaces',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        '../../mojo/public/cpp/bindings/tests/bindings_perftest.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:mojo_public_environment_unittests
      'target_name': 'mojo_public_environment_unittests',
      'type': 'executable',
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public.gyp:mojo_environment_standalone',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public.gyp:mojo_utility',
      ],
      'include_dirs': [ '../..' ],
      'sources': [
        '../../mojo/public/cpp/environment/tests/async_wait_unittest.cc',
        '../../mojo/public/cpp/environment/tests/async_waiter_unittest.cc',
        '../../mojo/public/cpp/environment/tests/logger_unittest.cc',
        '../../mojo/public/cpp/environment/tests/logging_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/public/cpp/system/tests:mojo_public_system_unittests
      # and         //mojo/public/c/system/tests
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_public_test_utils',
      ],
      'include_dirs': [ '../..' ],
      'sources': [
        '<@(mojo_public_system_unittest_sources)',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application/tests:mojo_public_utility_unittests
      'target_name': 'mojo_public_utility_unittests',
      'type': 'executable',
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public.gyp:mojo_utility',
      ],
      'include_dirs': [ '../..' ],
      'sources': [
        '../../mojo/public/cpp/utility/tests/mutex_unittest.cc',
        '../../mojo/public/cpp/utility/tests/run_loop_unittest.cc',
        '../../mojo/public/cpp/utility/tests/thread_unittest.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            '../../mojo/public/cpp/utility/tests/mutex_unittest.cc',
            '../../mojo/public/cpp/utility/tests/thread_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/test:mojo_public_system_perftests
      'target_name': 'mojo_public_system_perftests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_run_all_perftests',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        '../../mojo/public/c/system/tests/core_perftest.cc',
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_system_unittests
      'target_name': 'mojo_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_system_impl',
      ],
      'sources': [
        'src/mojo/edk/embedder/embedder_unittest.cc',
        'src/mojo/edk/embedder/platform_channel_pair_posix_unittest.cc',
        'src/mojo/edk/embedder/simple_platform_shared_buffer_unittest.cc',
        'src/mojo/edk/system/awakable_list_unittest.cc',
        'src/mojo/edk/system/channel_endpoint_id_unittest.cc',
        'src/mojo/edk/system/channel_endpoint_unittest.cc',
        'src/mojo/edk/system/channel_manager_unittest.cc',
        'src/mojo/edk/system/channel_test_base.cc',
        'src/mojo/edk/system/channel_test_base.h',
        'src/mojo/edk/system/channel_unittest.cc',
        'src/mojo/edk/system/core_test_base.cc',
        'src/mojo/edk/system/core_test_base.h',
        'src/mojo/edk/system/core_unittest.cc',
        'src/mojo/edk/system/data_pipe_impl_unittest.cc',
        'src/mojo/edk/system/data_pipe_unittest.cc',
        'src/mojo/edk/system/dispatcher_unittest.cc',
        'src/mojo/edk/system/endpoint_relayer_unittest.cc',
        'src/mojo/edk/system/ipc_support_unittest.cc',
        'src/mojo/edk/system/memory_unittest.cc',
        'src/mojo/edk/system/message_in_transit_queue_unittest.cc',
        'src/mojo/edk/system/message_in_transit_test_utils.cc',
        'src/mojo/edk/system/message_in_transit_test_utils.h',
        'src/mojo/edk/system/message_pipe_dispatcher_unittest.cc',
        'src/mojo/edk/system/message_pipe_test_utils.cc',
        'src/mojo/edk/system/message_pipe_test_utils.h',
        'src/mojo/edk/system/message_pipe_unittest.cc',
        'src/mojo/edk/system/multiprocess_message_pipe_unittest.cc',
        'src/mojo/edk/system/mutex_unittest.cc',
        'src/mojo/edk/system/options_validation_unittest.cc',
        'src/mojo/edk/system/platform_handle_dispatcher_unittest.cc',
        'src/mojo/edk/system/raw_channel_unittest.cc',
        'src/mojo/edk/system/remote_data_pipe_impl_unittest.cc',
        'src/mojo/edk/system/remote_message_pipe_unittest.cc',
        'src/mojo/edk/system/run_all_unittests.cc',
        'src/mojo/edk/system/shared_buffer_dispatcher_unittest.cc',
        'src/mojo/edk/system/simple_dispatcher_unittest.cc',
        'src/mojo/edk/system/test_channel_endpoint_client.cc',
        'src/mojo/edk/system/test_channel_endpoint_client.h',
        'src/mojo/edk/system/test_utils.cc',
        'src/mojo/edk/system/test_utils.h',
        'src/mojo/edk/system/thread_annotations_unittest.cc',
        'src/mojo/edk/system/unique_identifier_unittest.cc',
        'src/mojo/edk/system/wait_set_dispatcher_unittest.cc',
        'src/mojo/edk/system/waiter_test_utils.cc',
        'src/mojo/edk/system/waiter_test_utils.h',
        'src/mojo/edk/system/waiter_unittest.cc',
        'src/mojo/edk/test/multiprocess_test_helper_unittest.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'src/mojo/edk/embedder/embedder_unittest.cc',
            'src/mojo/edk/system/ipc_support_unittest.cc',
            'src/mojo/edk/system/multiprocess_message_pipe_unittest.cc',
            'src/mojo/edk/test/multiprocess_test_helper_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_message_pipe_perftests
      'target_name': 'mojo_message_pipe_perftests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../base/base.gyp:test_support_perf',
        '../../testing/gtest.gyp:gtest',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_system_impl',
      ],
      'sources': [
        'src/mojo/edk/system/message_pipe_perftest.cc',
        'src/mojo/edk/system/message_pipe_test_utils.cc',
        'src/mojo/edk/system/message_pipe_test_utils.h',
        'src/mojo/edk/system/test_utils.cc',
        'src/mojo/edk/system/test_utils.h',
      ],
    },
    {
      'target_name': 'mojo_js_to_cpp_bindings',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'src/mojo/edk/js/tests/js_to_cpp.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'mojo_public_bindings_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_public_bindings_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'mojo_public_bindings_unittests.isolate',
          ],
        },
        {
          'target_name': 'mojo_public_environment_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_public_environment_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'mojo_public_environment_unittests.isolate',
          ],
        },
        {
          'target_name': 'mojo_public_system_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_public_system_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'mojo_public_system_unittests.isolate',
          ],
        },
        {
          'target_name': 'mojo_public_utility_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_public_utility_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'mojo_public_utility_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
