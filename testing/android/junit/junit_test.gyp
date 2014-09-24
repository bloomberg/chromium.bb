# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'junit_test_support',
      'type': 'none',
      'dependencies': [
        '../../../third_party/junit/junit.gyp:junit_jar',
      ],
      'variables': {
        'src_paths': [
          'java/src',
        ],
      },
      'includes': [
        '../../../build/host_jar.gypi',
      ],
    },
    {
      'target_name': 'junit_unit_tests',
      'type': 'none',
      'dependencies': [
        'junit_test_support',
      ],
      'variables': {
        'main_class': 'org.chromium.testing.local.JunitTestMain',
        'src_paths': [
          'javatests/src',
        ],
      },
      'includes': [
        '../../../build/host_jar.gypi',
      ],
    },
  ],
}
