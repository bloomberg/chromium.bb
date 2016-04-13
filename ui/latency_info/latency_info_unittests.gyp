# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/latency_info:latency_info_unittests
      'target_name': 'latency_info_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/ipc/ipc.gyp:test_support_ipc',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'latency_info.gyp:latency_info',
        'latency_info.gyp:latency_info_ipc',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'latency_info_unittest.cc',
        'ipc/latency_info_param_traits_unittest.cc',
      ],
      'include_dirs': [
        '../../testing/gmock/include',
      ],
    },
  ],
}
