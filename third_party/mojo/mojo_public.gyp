# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'target_defaults' : {
    'include_dirs': [
      'src',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        'src',
      ],
    },
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
        'src/mojo/public/platform/native/system_thunks.cc',
        'src/mojo/public/platform/native/system_thunks.h',
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
        'src/mojo/public/c/system/buffer.h',
        'src/mojo/public/c/system/core.h',
        'src/mojo/public/c/system/data_pipe.h',
        'src/mojo/public/c/system/functions.h',
        'src/mojo/public/c/system/macros.h',
        'src/mojo/public/c/system/message_pipe.h',
        'src/mojo/public/c/system/system_export.h',
        'src/mojo/public/c/system/types.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/system
      'target_name': 'mojo_system_cpp_headers',
      'type': 'none',
      'sources': [
        'src/mojo/public/cpp/system/buffer.h',
        'src/mojo/public/cpp/system/core.h',
        'src/mojo/public/cpp/system/data_pipe.h',
        'src/mojo/public/cpp/system/functions.h',
        'src/mojo/public/cpp/system/handle.h',
        'src/mojo/public/cpp/system/macros.h',
        'src/mojo/public/cpp/system/message_pipe.h',
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
        'src/mojo/public/cpp/bindings/array.h',
        'src/mojo/public/cpp/bindings/binding.h',
        'src/mojo/public/cpp/bindings/callback.h',
        'src/mojo/public/cpp/bindings/interface_ptr.h',
        'src/mojo/public/cpp/bindings/interface_request.h',
        'src/mojo/public/cpp/bindings/message.h',
        'src/mojo/public/cpp/bindings/message_filter.h',
        'src/mojo/public/cpp/bindings/no_interface.h',
        'src/mojo/public/cpp/bindings/string.h',
        'src/mojo/public/cpp/bindings/strong_binding.h',
        'src/mojo/public/cpp/bindings/type_converter.h',
        'src/mojo/public/cpp/bindings/lib/array_internal.h',
        'src/mojo/public/cpp/bindings/lib/array_internal.cc',
        'src/mojo/public/cpp/bindings/lib/array_serialization.h',
        'src/mojo/public/cpp/bindings/lib/bindings_internal.h',
        'src/mojo/public/cpp/bindings/lib/bindings_serialization.cc',
        'src/mojo/public/cpp/bindings/lib/bindings_serialization.h',
        'src/mojo/public/cpp/bindings/lib/bounds_checker.cc',
        'src/mojo/public/cpp/bindings/lib/bounds_checker.h',
        'src/mojo/public/cpp/bindings/lib/buffer.h',
        'src/mojo/public/cpp/bindings/lib/callback_internal.h',
        'src/mojo/public/cpp/bindings/lib/connector.cc',
        'src/mojo/public/cpp/bindings/lib/connector.h',
        'src/mojo/public/cpp/bindings/lib/control_message_handler.cc',
        'src/mojo/public/cpp/bindings/lib/control_message_handler.h',
        'src/mojo/public/cpp/bindings/lib/control_message_proxy.cc',
        'src/mojo/public/cpp/bindings/lib/control_message_proxy.h',
        'src/mojo/public/cpp/bindings/lib/filter_chain.cc',
        'src/mojo/public/cpp/bindings/lib/filter_chain.h',
        'src/mojo/public/cpp/bindings/lib/fixed_buffer.cc',
        'src/mojo/public/cpp/bindings/lib/fixed_buffer.h',
        'src/mojo/public/cpp/bindings/lib/interface_ptr_internal.h',
        'src/mojo/public/cpp/bindings/lib/map_data_internal.h',
        'src/mojo/public/cpp/bindings/lib/map_internal.h',
        'src/mojo/public/cpp/bindings/lib/map_serialization.h',
        'src/mojo/public/cpp/bindings/lib/message.cc',
        'src/mojo/public/cpp/bindings/lib/message_builder.cc',
        'src/mojo/public/cpp/bindings/lib/message_builder.h',
        'src/mojo/public/cpp/bindings/lib/message_filter.cc',
        'src/mojo/public/cpp/bindings/lib/message_header_validator.cc',
        'src/mojo/public/cpp/bindings/lib/message_header_validator.h',
        'src/mojo/public/cpp/bindings/lib/message_internal.h',
        'src/mojo/public/cpp/bindings/lib/no_interface.cc',
        'src/mojo/public/cpp/bindings/lib/router.cc',
        'src/mojo/public/cpp/bindings/lib/router.h',
        'src/mojo/public/cpp/bindings/lib/shared_data.h',
        'src/mojo/public/cpp/bindings/lib/shared_ptr.h',
        'src/mojo/public/cpp/bindings/lib/string_serialization.h',
        'src/mojo/public/cpp/bindings/lib/string_serialization.cc',
        'src/mojo/public/cpp/bindings/lib/validate_params.h',
        'src/mojo/public/cpp/bindings/lib/validation_errors.cc',
        'src/mojo/public/cpp/bindings/lib/validation_errors.h',
        'src/mojo/public/cpp/bindings/lib/validation_util.cc',
        'src/mojo/public/cpp/bindings/lib/validation_util.h',
        'src/mojo/public/cpp/bindings/lib/value_traits.h',
        # This comes from the mojo_interface_bindings_cpp_sources dependency.
        '>@(mojom_generated_sources)',
      ],
      'dependencies': [
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
        'src/mojo/public/js/constants.cc',
        'src/mojo/public/js/constants.h',
      ],
    },
    {
      # GN version: //mojo/public/cpp/environment:standalone
      'target_name': 'mojo_environment_standalone',
      'type': 'static_library',
      'sources': [
        'src/mojo/public/c/environment/async_waiter.h',
        'src/mojo/public/c/environment/logger.h',
        'src/mojo/public/cpp/environment/async_waiter.h',
        'src/mojo/public/cpp/environment/environment.h',
        'src/mojo/public/cpp/environment/lib/async_waiter.cc',
        'src/mojo/public/cpp/environment/lib/default_async_waiter.cc',
        'src/mojo/public/cpp/environment/lib/default_async_waiter.h',
        'src/mojo/public/cpp/environment/lib/default_logger.cc',
        'src/mojo/public/cpp/environment/lib/default_logger.h',
        'src/mojo/public/cpp/environment/lib/default_task_tracker.cc',
        'src/mojo/public/cpp/environment/lib/default_task_tracker.h',
        'src/mojo/public/cpp/environment/lib/environment.cc',
        'src/mojo/public/cpp/environment/lib/logging.cc',
        'src/mojo/public/cpp/environment/lib/scoped_task_tracking.cc',
        'src/mojo/public/cpp/environment/lib/scoped_task_tracking.h',
        'src/mojo/public/cpp/environment/logging.h',
        'src/mojo/public/cpp/environment/task_tracker.h',
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
        'src/mojo/public/cpp/utility/mutex.h',
        'src/mojo/public/cpp/utility/run_loop.h',
        'src/mojo/public/cpp/utility/run_loop_handler.h',
        'src/mojo/public/cpp/utility/thread.h',
        'src/mojo/public/cpp/utility/lib/mutex.cc',
        'src/mojo/public/cpp/utility/lib/run_loop.cc',
        'src/mojo/public/cpp/utility/lib/thread.cc',
        'src/mojo/public/cpp/utility/lib/thread_local.h',
        'src/mojo/public/cpp/utility/lib/thread_local_posix.cc',
        'src/mojo/public/cpp/utility/lib/thread_local_win.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'src/mojo/public/cpp/utility/mutex.h',
            'src/mojo/public/cpp/utility/thread.h',
            'src/mojo/public/cpp/utility/lib/mutex.cc',
            'src/mojo/public/cpp/utility/lib/thread.cc',
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
          'src/mojo/public/interfaces/bindings/interface_control_messages.mojom',
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
        'src/mojo/public/c/test_support/test_support.h',
        'src/mojo/public/c/test_support/test_support_export.h',
        # TODO(vtl): Convert this to thunks http://crbug.com/386799
        'src/mojo/public/tests/test_support_private.cc',
        'src/mojo/public/tests/test_support_private.h',
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
        'src/mojo/public/cpp/test_support/lib/test_support.cc',
        'src/mojo/public/cpp/test_support/lib/test_utils.cc',
        'src/mojo/public/cpp/test_support/test_utils.h',
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
        'src/mojo/public/cpp/bindings/tests/validation_test_input_parser.cc',
        'src/mojo/public/cpp/bindings/tests/validation_test_input_parser.h',
      ],
    },
    {
      'target_name': 'mojo_public_test_interfaces_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'src/mojo/public/interfaces/bindings/tests/math_calculator.mojom',
          'src/mojo/public/interfaces/bindings/tests/no_module.mojom',
          'src/mojo/public/interfaces/bindings/tests/ping_service.mojom',
          'src/mojo/public/interfaces/bindings/tests/rect.mojom',
          'src/mojo/public/interfaces/bindings/tests/regression_tests.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_factory.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_import.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_import2.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_interfaces.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_service.mojom',
          'src/mojo/public/interfaces/bindings/tests/scoping.mojom',
          'src/mojo/public/interfaces/bindings/tests/serialization_test_structs.mojom',
          'src/mojo/public/interfaces/bindings/tests/test_structs.mojom',
          'src/mojo/public/interfaces/bindings/tests/validation_test_interfaces.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
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
            'java_in_dir': 'src/mojo/public/java/system',
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
            'java_in_dir': 'src/mojo/public/java/bindings',
           },
           'dependencies': [
             'mojo_interface_bindings_java_sources',
             'mojo_public_java',
           ],
           'includes': [ '../../build/java.gypi' ],
        },
      ],
    }],
  ],
}
