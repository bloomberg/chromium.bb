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
        'os_compatibility.cc',
        'os_compatibility.h',
        'policy.cc',
        'policy.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'SANDBOX_IMPLEMENTATION',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/usr/lib/libbsm.dylib',
        ],
      },
    },
    {
      'target_name': 'sandbox_mac_unittests',
      'type': 'executable',
      'sources': [
        'bootstrap_sandbox_unittest.mm',
        'policy_unittest.cc',
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
}
