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
      # GN version: //mojo/public/c/system
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
        'src/mojo/public/c/system/buffer.h',
        'src/mojo/public/c/system/core.h',
        'src/mojo/public/c/system/data_pipe.h',
        'src/mojo/public/c/system/functions.h',
        'src/mojo/public/c/system/macros.h',
        'src/mojo/public/c/system/message_pipe.h',
        'src/mojo/public/c/system/system_export.h',
        'src/mojo/public/c/system/types.h',
        'src/mojo/public/platform/native/system_thunks.cc',
        'src/mojo/public/platform/native/system_thunks.h',
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
        'src/mojo/public/cpp/bindings/error_handler.h',
        'src/mojo/public/cpp/bindings/interface_impl.h',
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
        'src/mojo/public/cpp/bindings/lib/message_queue.cc',
        'src/mojo/public/cpp/bindings/lib/message_queue.h',
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
        'src/mojo/public/cpp/environment/lib/environment.cc',
        'src/mojo/public/cpp/environment/lib/logging.cc',
        'src/mojo/public/cpp/environment/logging.h',
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
      'target_name': 'mojo_application_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'src/mojo/public/interfaces/application/application.mojom',
          'src/mojo/public/interfaces/application/service_provider.mojom',
          'src/mojo/public/interfaces/application/shell.mojom',
        ],
      },
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //mojo/public/interfaces/application:application
      'target_name': 'mojo_application_bindings',
      'type': 'static_library',
      'dependencies': [
        'mojo_application_bindings_mojom',
        'mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application
      'target_name': 'mojo_application_base',
      'type': 'static_library',
      'sources': [
        'src/mojo/public/cpp/application/application_connection.h',
        'src/mojo/public/cpp/application/application_delegate.h',
        'src/mojo/public/cpp/application/application_impl.h',
        'src/mojo/public/cpp/application/connect.h',
        'src/mojo/public/cpp/application/interface_factory.h',
        'src/mojo/public/cpp/application/interface_factory_impl.h',
        'src/mojo/public/cpp/application/lib/application_connection.cc',
        'src/mojo/public/cpp/application/lib/application_delegate.cc',
        'src/mojo/public/cpp/application/lib/application_impl.cc',
        'src/mojo/public/cpp/application/lib/service_provider_impl.cc',
        'src/mojo/public/cpp/application/lib/service_connector.cc',
        'src/mojo/public/cpp/application/lib/service_connector.h',
        'src/mojo/public/cpp/application/lib/service_registry.cc',
        'src/mojo/public/cpp/application/lib/service_registry.h',
        'src/mojo/public/cpp/application/service_provider_impl.h',
      ],
      'dependencies': [
        'mojo_application_bindings',
      ],
      'export_dependent_settings': [
        'mojo_application_bindings',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application:standalone"
      'target_name': 'mojo_application_standalone',
      'type': 'static_library',
      'sources': [
        'src/mojo/public/cpp/application/lib/application_runner.cc',
        'src/mojo/public/cpp/application/application_runner.h',
      ],
      'dependencies': [
        'mojo_application_base',
        'mojo_environment_standalone',
      ],
      'export_dependent_settings': [
        'mojo_application_base',
      ],
    },
    {
      # GN version: //mojo/public/c/test_support
      'target_name': 'mojo_test_support',
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
        'mojo_test_support',
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
          'src/mojo/public/interfaces/bindings/tests/rect.mojom',
          'src/mojo/public/interfaces/bindings/tests/regression_tests.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_factory.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_import.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_import2.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_interfaces.mojom',
          'src/mojo/public/interfaces/bindings/tests/sample_service.mojom',
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
          # GN version: //mojo/public/java_system
          'target_name': 'mojo_public_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'src/mojo/public/java/system',
          },
          'includes': [ '../../build/java.gypi' ],
        },
        {
        # GN version: //mojo/public/java_bindings
          'target_name': 'mojo_bindings_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'src/mojo/public/java/bindings',
           },
           'dependencies': [
             'mojo_public_java',
           ],
           'includes': [ '../../build/java.gypi' ],
        },
      ],
    }],
  ],
}
