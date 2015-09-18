# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'sandbox',
      'type': '<(component)',
      'sources': [
        'bootstrap_sandbox.cc',
        'bootstrap_sandbox.h',
        'launchd_interception_server.cc',
        'launchd_interception_server.h',
        'mach_message_server.cc',
        'mach_message_server.h',
        'message_server.h',
        'os_compatibility.cc',
        'os_compatibility.h',
        'policy.cc',
        'policy.h',
        'pre_exec_delegate.cc',
        'pre_exec_delegate.h',
        'xpc.cc',
        'xpc.h',
        'xpc_message_server.cc',
        'xpc_message_server.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'defines': [
        'SANDBOX_IMPLEMENTATION',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/usr/lib/libbsm.dylib',
        ],
      },
      'conditions': [
        # When the build SDK is 10.6, generate a dynamic stub loader. When the
        # SDK is higher, then libxpc.dylib will be loaded automatically as part
        # of libSystem, and only forward declarations of private symbols are
        # necessary.
        ['mac_sdk == "10.6"', {
          'actions': [
            {
              'variables': {
                'generate_stubs_script': '../tools/generate_stubs/generate_stubs.py',
                'generate_stubs_header_path': 'xpc_stubs_header.fragment',
                'generate_stubs_sig_public_path': 'xpc_stubs.sig',
                'generate_stubs_sig_private_path': 'xpc_private_stubs.sig',
                'generate_stubs_project': 'sandbox/mac',
                'generate_stubs_output_stem': 'xpc_stubs',
              },
              'action_name': 'generate_stubs',
              'inputs': [
                '<(generate_stubs_script)',
                '<(generate_stubs_header_path)',
                '<(generate_stubs_sig_public_path)',
                '<(generate_stubs_sig_private_path)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/<(generate_stubs_output_stem).cc',
                '<(SHARED_INTERMEDIATE_DIR)/<(generate_stubs_project)/<(generate_stubs_output_stem).h',
              ],
              'action': [
                'python',
                '<(generate_stubs_script)',
                '-i', '<(INTERMEDIATE_DIR)',
                '-o', '<(SHARED_INTERMEDIATE_DIR)/<(generate_stubs_project)',
                '-t', 'posix_stubs',
                '-e', '<(generate_stubs_header_path)',
                '-s', '<(generate_stubs_output_stem)',
                '-p', '<(generate_stubs_project)',
                '-x', 'SANDBOX_EXPORT',
                '<(generate_stubs_sig_public_path)',
                '<(generate_stubs_sig_private_path)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating XPC stubs for 10.6 compatability.',
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'sandbox_mac_unittests',
      'type': 'executable',
      'sources': [
        'bootstrap_sandbox_unittest.mm',
        'policy_unittest.cc',
        'xpc_message_server_unittest.cc',
      ],
      'dependencies': [
        'sandbox',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
        ],
      },
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'sandbox_mac_unittests_run',
          'type': 'none',
          'dependencies': [
            'sandbox_mac_unittests',
          ],
          'includes': [ '../../build/isolate.gypi' ],
          'sources': [ '../sandbox_mac_unittests.isolate' ],
        },
      ],
    }],
  ],
}
