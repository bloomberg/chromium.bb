# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'target_defaults' : {
    'include_dirs': [
      '../..',
    ],
  },
  'targets': [
    {
      'target_name': 'mojo_public',
      'type': 'none',
      'dependencies': [
        'mojo_js_bindings',
        'mojo_public_test_interfaces',
        'mojo_public_test_utils',
        'mojo_system',
        'mojo_utility',
      ],
    },
    {
      # Targets that (a) need to obtain the settings that mojo_system passes on
      # to its direct dependents but (b) are not themselves in a position to
      # hardcode a dependency to mojo_system vs. mojo_system_impl (e.g.,
      # because they are components) should depend on this target.
      'target_name': 'mojo_system_placeholder',
      'type': 'none',
    },
    {
      'target_name': 'mojo_system',
      'type': 'static_library',
      'defines': [
        'MOJO_SYSTEM_IMPLEMENTATION',
      ],
      'all_dependent_settings': {
        'conditions': [
          # We need to be able to call the MojoSetSystemThunks() function in
          # system_thunks.cc
          ['OS=="android"', {
            'ldflags!': [
              '-Wl,--exclude-libs=ALL',
            ],
          }],
        ],
      },
      'sources': [
        '../../mojo/public/platform/native/system_thunks.cc',
        '../../mojo/public/platform/native/system_thunks.h',
      ],
      'dependencies': [
        'mojo_system_headers',
      ],
    },
    {
      # GN version: //mojo/public/c/system
      'target_name': 'mojo_system_headers',
      'type': 'none',
      'sources': [
        '../../mojo/public/c/system/buffer.h',
        '../../mojo/public/c/system/core.h',
        '../../mojo/public/c/system/data_pipe.h',
        '../../mojo/public/c/system/functions.h',
        '../../mojo/public/c/system/macros.h',
        '../../mojo/public/c/system/message_pipe.h',
        '../../mojo/public/c/system/system_export.h',
        '../../mojo/public/c/system/types.h',
        '../../mojo/public/c/system/wait_set.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/system
      'target_name': 'mojo_system_cpp_headers',
      'type': 'none',
      'sources': [
        '../../mojo/public/cpp/system/buffer.h',
        '../../mojo/public/cpp/system/core.h',
        '../../mojo/public/cpp/system/data_pipe.h',
        '../../mojo/public/cpp/system/functions.h',
        '../../mojo/public/cpp/system/handle.h',
        '../../mojo/public/cpp/system/macros.h',
        '../../mojo/public/cpp/system/message_pipe.h',
      ],
      'dependencies': [
        'mojo_system_headers',
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings
      'target_name': 'mojo_cpp_bindings',
      'type': 'static_library',
      'include_dirs': [
        '../..'
      ],
      'sources': [
        '../../mojo/public/cpp/bindings/array.h',
        '../../mojo/public/cpp/bindings/associated_binding.h',
        '../../mojo/public/cpp/bindings/associated_group.h',
        '../../mojo/public/cpp/bindings/associated_interface_ptr.h',
        '../../mojo/public/cpp/bindings/associated_interface_ptr_info.h',
        '../../mojo/public/cpp/bindings/associated_interface_request.h',
        '../../mojo/public/cpp/bindings/binding.h',
        '../../mojo/public/cpp/bindings/callback.h',
        '../../mojo/public/cpp/bindings/interface_ptr.h',
        '../../mojo/public/cpp/bindings/interface_request.h',
        '../../mojo/public/cpp/bindings/lib/array_internal.cc',
        '../../mojo/public/cpp/bindings/lib/array_internal.h',
        '../../mojo/public/cpp/bindings/lib/array_serialization.h',
        '../../mojo/public/cpp/bindings/lib/associated_group.cc',
        '../../mojo/public/cpp/bindings/lib/associated_interface_ptr_state.h',
        '../../mojo/public/cpp/bindings/lib/binding_state.h',
        '../../mojo/public/cpp/bindings/lib/bindings_internal.h',
        '../../mojo/public/cpp/bindings/lib/bindings_serialization.cc',
        '../../mojo/public/cpp/bindings/lib/bindings_serialization.h',
        '../../mojo/public/cpp/bindings/lib/bounds_checker.cc',
        '../../mojo/public/cpp/bindings/lib/bounds_checker.h',
        '../../mojo/public/cpp/bindings/lib/buffer.h',
        '../../mojo/public/cpp/bindings/lib/callback_internal.h',
        '../../mojo/public/cpp/bindings/lib/connector.cc',
        '../../mojo/public/cpp/bindings/lib/connector.h',
        '../../mojo/public/cpp/bindings/lib/control_message_handler.cc',
        '../../mojo/public/cpp/bindings/lib/control_message_handler.h',
        '../../mojo/public/cpp/bindings/lib/control_message_proxy.cc',
        '../../mojo/public/cpp/bindings/lib/control_message_proxy.h',
        '../../mojo/public/cpp/bindings/lib/filter_chain.cc',
        '../../mojo/public/cpp/bindings/lib/filter_chain.h',
        '../../mojo/public/cpp/bindings/lib/fixed_buffer.cc',
        '../../mojo/public/cpp/bindings/lib/interface_id.h',
        '../../mojo/public/cpp/bindings/lib/fixed_buffer.h',
        '../../mojo/public/cpp/bindings/lib/interface_endpoint_client.cc',
        '../../mojo/public/cpp/bindings/lib/interface_endpoint_client.h',
        '../../mojo/public/cpp/bindings/lib/interface_ptr_state.h',
        '../../mojo/public/cpp/bindings/lib/map_data_internal.h',
        '../../mojo/public/cpp/bindings/lib/map_internal.h',
        '../../mojo/public/cpp/bindings/lib/map_serialization.h',
        '../../mojo/public/cpp/bindings/lib/message.cc',
        '../../mojo/public/cpp/bindings/lib/message_builder.cc',
        '../../mojo/public/cpp/bindings/lib/message_builder.h',
        '../../mojo/public/cpp/bindings/lib/message_filter.cc',
        '../../mojo/public/cpp/bindings/lib/message_header_validator.cc',
        '../../mojo/public/cpp/bindings/lib/message_header_validator.h',
        '../../mojo/public/cpp/bindings/lib/message_internal.h',
        '../../mojo/public/cpp/bindings/lib/multiplex_router.cc',
        '../../mojo/public/cpp/bindings/lib/multiplex_router.h',
        '../../mojo/public/cpp/bindings/lib/no_interface.cc',
        '../../mojo/public/cpp/bindings/lib/pickle_buffer.cc',
        '../../mojo/public/cpp/bindings/lib/pickle_buffer.h',
        '../../mojo/public/cpp/bindings/lib/pipe_control_message_handler.cc',
        '../../mojo/public/cpp/bindings/lib/pipe_control_message_handler.h',
        '../../mojo/public/cpp/bindings/lib/pipe_control_message_handler_delegate.h',
        '../../mojo/public/cpp/bindings/lib/pipe_control_message_proxy.cc',
        '../../mojo/public/cpp/bindings/lib/pipe_control_message_proxy.h',
        '../../mojo/public/cpp/bindings/lib/router.cc',
        '../../mojo/public/cpp/bindings/lib/router.h',
        '../../mojo/public/cpp/bindings/lib/scoped_interface_endpoint_handle.cc',
        '../../mojo/public/cpp/bindings/lib/scoped_interface_endpoint_handle.h',
        '../../mojo/public/cpp/bindings/lib/shared_data.h',
        '../../mojo/public/cpp/bindings/lib/shared_ptr.h',
        '../../mojo/public/cpp/bindings/lib/string_serialization.cc',
        '../../mojo/public/cpp/bindings/lib/string_serialization.h',
        '../../mojo/public/cpp/bindings/lib/validate_params.h',
        '../../mojo/public/cpp/bindings/lib/validation_errors.cc',
        '../../mojo/public/cpp/bindings/lib/validation_errors.h',
        '../../mojo/public/cpp/bindings/lib/validation_util.cc',
        '../../mojo/public/cpp/bindings/lib/validation_util.h',
        '../../mojo/public/cpp/bindings/lib/value_traits.h',
        '../../mojo/public/cpp/bindings/message.h',
        '../../mojo/public/cpp/bindings/message_filter.h',
        '../../mojo/public/cpp/bindings/no_interface.h',
        '../../mojo/public/cpp/bindings/string.h',
        '../../mojo/public/cpp/bindings/strong_binding.h',
        '../../mojo/public/cpp/bindings/type_converter.h',
        # This comes from the mojo_interface_bindings_cpp_sources dependency.
        '>@(mojom_generated_sources)',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        'mojo_interface_bindings_cpp_sources',
      ],
    },
    {
      # GN version: //mojo/public/js
      'target_name': 'mojo_js_bindings',
      'type': 'static_library',
      'include_dirs': [
        '../..'
      ],
      'sources': [
        '../../mojo/public/js/constants.cc',
        '../../mojo/public/js/constants.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/environment:standalone
      'target_name': 'mojo_environment_standalone',
      'type': 'static_library',
      'sources': [
        '../../mojo/public/c/environment/async_waiter.h',
        '../../mojo/public/c/environment/logger.h',
        '../../mojo/public/cpp/environment/async_waiter.h',
        '../../mojo/public/cpp/environment/environment.h',
        '../../mojo/public/cpp/environment/lib/async_waiter.cc',
        '../../mojo/public/cpp/environment/lib/default_async_waiter.cc',
        '../../mojo/public/cpp/environment/lib/default_async_waiter.h',
        '../../mojo/public/cpp/environment/lib/default_logger.cc',
        '../../mojo/public/cpp/environment/lib/default_logger.h',
        '../../mojo/public/cpp/environment/lib/default_task_tracker.cc',
        '../../mojo/public/cpp/environment/lib/default_task_tracker.h',
        '../../mojo/public/cpp/environment/lib/environment.cc',
        '../../mojo/public/cpp/environment/lib/logging.cc',
        '../../mojo/public/cpp/environment/lib/scoped_task_tracking.cc',
        '../../mojo/public/cpp/environment/lib/scoped_task_tracking.h',
        '../../mojo/public/cpp/environment/logging.h',
        '../../mojo/public/cpp/environment/task_tracker.h',
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      # GN version: //mojo/public/cpp/utility
      'target_name': 'mojo_utility',
      'type': 'static_library',
      'sources': [
        '../../mojo/public/cpp/utility/lib/mutex.cc',
        '../../mojo/public/cpp/utility/lib/run_loop.cc',
        '../../mojo/public/cpp/utility/lib/thread.cc',
        '../../mojo/public/cpp/utility/lib/thread_local.h',
        '../../mojo/public/cpp/utility/lib/thread_local_posix.cc',
        '../../mojo/public/cpp/utility/lib/thread_local_win.cc',
        '../../mojo/public/cpp/utility/mutex.h',
        '../../mojo/public/cpp/utility/run_loop.h',
        '../../mojo/public/cpp/utility/run_loop_handler.h',
        '../../mojo/public/cpp/utility/thread.h',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            '../../mojo/public/cpp/utility/lib/mutex.cc',
            '../../mojo/public/cpp/utility/lib/thread.cc',
            '../../mojo/public/cpp/utility/mutex.h',
            '../../mojo/public/cpp/utility/thread.h',
          ],
        }],
      ],
      'include_dirs': [
        '../..',
      ],
    },
    {
      'target_name': 'mojo_interface_bindings_mojom',
      'type': 'none',
      'variables': {
        'require_interface_bindings': 0,
        'mojom_files': [
          '../../mojo/public/interfaces/bindings/interface_control_messages.mojom',
          '../../mojo/public/interfaces/bindings/pipe_control_messages.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'mojo_interface_bindings_cpp_sources',
      'type': 'none',
      'dependencies': [
        'mojo_interface_bindings_mojom',
      ],
    },
    {
      # This target can be used to introduce a dependency on interface bindings
      # generation without introducing any side-effects in the dependent
      # target's configuration.
      'target_name': 'mojo_interface_bindings_generation',
      'type': 'none',
      'dependencies': [
        'mojo_interface_bindings_cpp_sources',
      ],
    },
    {
      # GN version: //mojo/public/c/test_support
      'target_name': 'mojo_public_test_support',
      'defines': [
        'MOJO_TEST_SUPPORT_IMPLEMENTATION',
      ],
      'include_dirs': [
        '../..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../..',
        ],
      },
      'sources': [
        '../../mojo/public/c/test_support/test_support.h',
        '../../mojo/public/c/test_support/test_support_export.h',
        # TODO(vtl): Convert this to thunks http://crbug.com/386799
        '../../mojo/public/tests/test_support_private.cc',
        '../../mojo/public/tests/test_support_private.h',
      ],
      'conditions': [
        ['OS=="ios"', {
          'type': 'static_library',
        }, {
          'type': 'shared_library',
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@loader_path',
          },
        }],
      ],
    },
    {
      # GN version: //mojo/public/cpp/test_support:test_utils
      'target_name': 'mojo_public_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
        'mojo_public_test_support',
      ],
      'sources': [
        '../../mojo/public/cpp/test_support/lib/test_support.cc',
        '../../mojo/public/cpp/test_support/lib/test_utils.cc',
        '../../mojo/public/cpp/test_support/test_utils.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/bindings/tests:mojo_public_bindings_test_utils
      'target_name': 'mojo_public_bindings_test_utils',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        '../../mojo/public/cpp/bindings/tests/validation_test_input_parser.cc',
        '../../mojo/public/cpp/bindings/tests/validation_test_input_parser.h',
      ],
    },
    {
      'target_name': 'mojo_public_test_interfaces_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          '../../mojo/public/interfaces/bindings/tests/math_calculator.mojom',
          '../../mojo/public/interfaces/bindings/tests/no_module.mojom',
          '../../mojo/public/interfaces/bindings/tests/ping_service.mojom',
          '../../mojo/public/interfaces/bindings/tests/rect.mojom',
          '../../mojo/public/interfaces/bindings/tests/regression_tests.mojom',
          '../../mojo/public/interfaces/bindings/tests/sample_factory.mojom',
          '../../mojo/public/interfaces/bindings/tests/sample_import.mojom',
          '../../mojo/public/interfaces/bindings/tests/sample_import2.mojom',
          '../../mojo/public/interfaces/bindings/tests/sample_interfaces.mojom',
          '../../mojo/public/interfaces/bindings/tests/sample_service.mojom',
          '../../mojo/public/interfaces/bindings/tests/scoping.mojom',
          '../../mojo/public/interfaces/bindings/tests/serialization_test_structs.mojom',
          '../../mojo/public/interfaces/bindings/tests/test_constants.mojom',
          '../../mojo/public/interfaces/bindings/tests/test_native_types.mojom',
          '../../mojo/public/interfaces/bindings/tests/test_structs.mojom',
          '../../mojo/public/interfaces/bindings/tests/test_unions.mojom',
          '../../mojo/public/interfaces/bindings/tests/validation_test_interfaces.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      'target_name': 'mojo_public_test_interfaces_mojom_blink',
      'type': 'none',
      'variables': {
        'mojom_variant': 'blink',
        'mojom_extra_generator_args': [
          '--typemap', '<(DEPTH)/mojo/public/interfaces/bindings/tests/blink_test.typemap',
        ],
        'mojom_files': [
          '../../mojo/public/interfaces/bindings/tests/test_native_types.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
      'dependencies': [
        'mojo_public_test_interfaces_mojom',
      ],
    },
    {
      'target_name': 'mojo_public_test_interfaces_mojom_chromium',
      'type': 'none',
      'variables': {
        'mojom_variant': 'chromium',
        'mojom_extra_generator_args': [
          '--typemap', '<(DEPTH)/mojo/public/interfaces/bindings/tests/chromium_test.typemap',
        ],
        'mojom_files': [
          '../../mojo/public/interfaces/bindings/tests/test_native_types.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
      'dependencies': [
        'mojo_public_test_interfaces_mojom',
      ],
    },
    {
      # GN version: //mojo/public/interfaces/bindings/tests:test_interfaces
      'target_name': 'mojo_public_test_interfaces',
      'type': 'static_library',
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_public_test_interfaces_mojom',
        'mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/public/interfaces/bindings/tests:test_interfaces_blink
      'target_name': 'mojo_public_test_interfaces_blink',
      'type': 'static_library',
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_public_test_interfaces_mojom_blink',
        'mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/public/interfaces/bindings/tests:test_interfaces_chromium
      'target_name': 'mojo_public_test_interfaces_chromium',
      'type': 'static_library',
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_public_test_interfaces_mojom_chromium',
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_public_test_associated_interfaces_mojom',
      'type': 'none',
      'variables': {
        # These files are not included in the mojo_public_test_interfaces_mojom
        # target because associated interfaces are not supported by all bindings
        # languages yet.
        'mojom_files': [
          '../../mojo/public/interfaces/bindings/tests/test_associated_interfaces.mojom',
          '../../mojo/public/interfaces/bindings/tests/validation_test_associated_interfaces.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //mojo/public/interfaces/bindings/tests:test_associated_interfaces
      'target_name': 'mojo_public_test_associated_interfaces',
      'type': 'static_library',
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_public_test_associated_interfaces_mojom',
        'mojo_cpp_bindings',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN version: //mojo/public/java:system
          'target_name': 'mojo_public_java',
          'type': 'none',
          'variables': {
            'chromium_code': 0,
            'java_in_dir': '../../mojo/public/java/system',
          },
          'includes': [ '../../build/java.gypi' ],
        },
        {
          'target_name': 'mojo_interface_bindings_java_sources',
          'type': 'none',
          'dependencies': [
            'mojo_interface_bindings_mojom',
          ],
        },
        {
          # GN version: //mojo/public/java:bindings
          'target_name': 'mojo_bindings_java',
          'type': 'none',
          'variables': {
            'chromium_code': 0,
            'java_in_dir': '../../mojo/public/java/bindings',
           },
           'dependencies': [
             'mojo_interface_bindings_java_sources',
             'mojo_public_java',
             '<(DEPTH)/base/base.gyp:base_java',
           ],
           'includes': [ '../../build/java.gypi' ],
        },
      ],
    }],
  ],
}
