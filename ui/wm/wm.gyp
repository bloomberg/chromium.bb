# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'wm_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
        '../views/views.gyp:views',
      ],
      'sources': [
        'test/wm_test_helper.cc',
        'test/wm_test_helper.h',
      ],
    },
  ],
}
