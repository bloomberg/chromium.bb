# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'sandbox_mac_unittests',
      'type': 'executable',
      'sources': [
        'temp_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../',
      ],
    },
  ],
}
