# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ui_ios_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        'ui_ios.gyp:ui_ios',
      ],
      'sources' : [
        'NSString+CrStringDrawing_unittest.mm',
        'uikit_util_unittest.mm',
      ],
    },
  ],
}