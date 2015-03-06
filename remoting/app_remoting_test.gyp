# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'remoting_version.gypi',
  ],

  'target_defaults': {
    'type': 'none',
  },  # target_defaults

  'targets': [
    {
      'target_name': 'ar_test_driver_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../remoting/remoting.gyp:remoting_base',
        '../remoting/remoting.gyp:remoting_client',
        '../remoting/remoting.gyp:remoting_protocol',
        '../remoting/proto/chromotocol.gyp:chromotocol_proto_lib',
        '../testing/gtest.gyp:gtest',
      ],
      'defines': [
        'VERSION=<(version_full)',
      ],
      'sources': [
        'test/access_token_fetcher.cc',
        'test/app_remoting_test_driver_environment.cc',
        'test/refresh_token_store.cc',
        'test/remote_host_info.cc',
        'test/remote_host_info_fetcher.cc',
      ],
    },  # end of target 'ar_test_driver_common'
    {
      # An external version of the test driver tool which includes minimal tests
      'target_name': 'ar_sample_test_driver',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'ar_test_driver_common',
      ],
      'defines': [
        'VERSION=<(version_full)',
      ],
      'sources': [
        'test/app_remoting_test_driver.cc',
      ],
      'include_dirs': [
        '../testing/gtest/include',
      ],
    },  # end of target 'ar_sample_test_driver'
  ],  # end of targets
}
