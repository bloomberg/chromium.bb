# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'wm_public',
      'type': 'static_library',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
        '../gfx/gfx.gyp:gfx_geometry',
      ],
      'sources': [
        'core/easy_resize_window_targeter.cc',
        'core/masked_window_targeter.cc',
        'public/easy_resize_window_targeter.h',
        'public/masked_window_targeter.h',
        'public/window_types.h',
      ],
    },
    {
      'target_name': 'wm_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
      ],
      'sources': [
        'test/wm_test_helper.cc',
        'test/wm_test_helper.h',
      ],
    },
  ],
}
