# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [{
    'target_name': 'shell_interfaces',
    'type': 'none',
    'variables': {
      'mojom_files': [
        '../catalog/public/interfaces/catalog.mojom',
        'public/interfaces/capabilities.mojom',
        'public/interfaces/connector.mojom',
        'public/interfaces/interface_provider.mojom',
        'public/interfaces/shell.mojom',
        'public/interfaces/shell_client.mojom',
        'public/interfaces/shell_client_factory.mojom',
        'public/interfaces/shell_resolver.mojom',
      ],
    },
    'includes': [ '../../mojo/mojom_bindings_generator_explicit.gypi' ],
  },
  {
    # GN version: //services/shell/public/cpp
    'target_name': 'shell_public',
    'type': 'static_library',
    'sources': [
      'public/cpp/application_runner.h',
      'public/cpp/capabilities.h',
      'public/cpp/connect.h',
      'public/cpp/connection.h',
      'public/cpp/connector.h',
      'public/cpp/identity.h',
      'public/cpp/initialize_base_and_icu.cc',
      'public/cpp/initialize_base_and_icu.h',
      'public/cpp/interface_binder.h',
      'public/cpp/interface_factory.h',
      'public/cpp/interface_factory_impl.h',
      'public/cpp/interface_registry.h',
      'public/cpp/lib/application_runner.cc',
      'public/cpp/lib/capabilities.cc',
      'public/cpp/lib/connection_impl.cc',
      'public/cpp/lib/connection_impl.h',
      'public/cpp/lib/connector_impl.cc',
      'public/cpp/lib/connector_impl.h',
      'public/cpp/lib/identity.cc',
      'public/cpp/lib/interface_factory_binder.h',
      'public/cpp/lib/interface_registry.cc',
      'public/cpp/lib/message_loop_ref.cc',
      'public/cpp/lib/names.cc',
      'public/cpp/lib/shell_client.cc',
      'public/cpp/lib/shell_connection.cc',
      'public/cpp/message_loop_ref.h',
      'public/cpp/names.h',
      'public/cpp/shell_client.h',
      'public/cpp/shell_connection.h',
    ],
    'dependencies': [
      'shell_interfaces',
      '<(DEPTH)/base/base.gyp:base_i18n',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_message_pump_lib',
    ],
  }, {
    'target_name': 'shell_lib',
    'type': 'static_library',
    'sources': [
      '../catalog/catalog.cc',
      '../catalog/catalog.h',
      '../catalog/constants.cc',
      '../catalog/constants.h',
      '../catalog/entry.cc',
      '../catalog/entry.h',
      '../catalog/instance.cc',
      '../catalog/instance.h',
      '../catalog/reader.cc',
      '../catalog/reader.h',
      '../catalog/store.cc',
      '../catalog/store.h',
      '../catalog/types.h',
      'connect_params.cc',
      'connect_params.h',
      'connect_util.cc',
      'connect_util.h',
      'native_runner.h',
      'native_runner_delegate.h',
      'shell.cc',
      'shell.h',
      'switches.cc',
      'switches.cc',
      '<(DEPTH)/mojo/util/filename_util.cc',
      '<(DEPTH)/mojo/util/filename_util.h',
    ],
    'dependencies': [
      'shell_public',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_url_type_converters',
      '<(DEPTH)/net/net.gyp:net',
      '<(DEPTH)/url/url.gyp:url_lib',
    ],
    'export_dependent_settings': [
      'shell_public',
    ],
  }, {
    'target_name': 'mojo_shell_unittests',
    'type': 'executable',
    'sources': [
      'tests/placeholder_unittest.cc',
    ],
    'dependencies': [
      'shell_lib',
      'shell_public',
      'shell_test_public',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
      '<(DEPTH)/mojo/mojo_base.gyp:mojo_url_type_converters',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_run_all_unittests',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
      '<(DEPTH)/testing/gtest.gyp:gtest',
      '<(DEPTH)/url/url.gyp:url_lib',
    ]
  }, {
    'target_name': 'shell_test_public',
    'type': 'static_library',
    'dependencies': [
      'shell_test_interfaces',
    ],
  }, {
    'target_name': 'shell_test_interfaces',
    'type': 'none',
    'variables': {
      'mojom_files': [
        'tests/test.mojom',
      ],
    },
    'includes': [
      '../../mojo/mojom_bindings_generator_explicit.gypi',
    ],
  }, {
    'target_name': 'shell_runner_common_lib',
    'type': 'static_library',
    'sources': [
      'runner/common/client_util.cc',
      'runner/common/client_util.h',
      'runner/common/switches.cc',
      'runner/common/switches.h',
    ],
    'include_dirs': [
      '..',
    ],
    'dependencies': [
      'shell_public',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_system',
    ],
    'export_dependent_settings': [
      'shell_public',
    ],
  }, {
    'target_name': 'shell_runner_host_lib',
    'type': 'static_library',
    'sources': [
      'runner/host/child_process.cc',
      'runner/host/child_process.h',
      'runner/host/child_process_base.cc',
      'runner/host/child_process_base.h',
      'runner/host/child_process_host.cc',
      'runner/host/child_process_host.h',
      'runner/host/in_process_native_runner.cc',
      'runner/host/in_process_native_runner.h',
      'runner/host/out_of_process_native_runner.cc',
      'runner/host/out_of_process_native_runner.h',
      'runner/host/native_application_support.cc',
      'runner/host/native_application_support.h',
      'runner/init.cc',
      'runner/init.h',
    ],
    'dependencies': [
      'shell_lib',
      'shell_public',
      'shell_runner_common_lib',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/base/base.gyp:base_i18n',
      '<(DEPTH)/base/base.gyp:base_static',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
      '<(DEPTH)/mojo/mojo_platform_handle.gyp:platform_handle',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_message_pump_lib',
    ],
    'export_dependent_settings': [
      'shell_public',
    ],
    'conditions': [
      ['OS=="linux"', {
        'sources': [
          'runner/host/linux_sandbox.cc',
          'runner/host/linux_sandbox.h',
        ],
        'dependencies': [
          '<(DEPTH)/sandbox/sandbox.gyp:sandbox',
          '<(DEPTH)/sandbox/sandbox.gyp:sandbox_services',
          '<(DEPTH)/sandbox/sandbox.gyp:seccomp_bpf',
          '<(DEPTH)/sandbox/sandbox.gyp:seccomp_bpf_helpers',
        ],
      }],
      ['OS=="mac"', {
        'sources': [
          'runner/host/mach_broker.cc',
          'runner/host/mach_broker.h',
        ],
      }],
    ],
  }, {
    # GN version: //services/catalog:manifest
    'target_name': 'catalog_manifest',
    'type': 'none',
    'variables': {
      'application_type': 'mojo',
      'application_name': 'catalog',
      'source_manifest': '<(DEPTH)/services/catalog/manifest.json',
    },
    'includes': [
      '../../mojo/public/mojo_application_manifest.gypi',
    ],
    'hard_dependency': 1,
  }, {
    # GN version: //services/shell/public/cpp/tests
    'target_name': 'shell_client_lib_unittests',
    'type': 'executable',
    'dependencies': [
      'shell_public',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_run_all_unittests',
      '<(DEPTH)/testing/gtest.gyp:gtest',
    ],
    'sources': [
      'public/cpp/tests/interface_registry_unittest.cc',
    ],
  }],
}
