# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'shell',
      'type': 'static_library',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '../aura/aura.gyp:aura',
        '../views/views.gyp:views',
      ],
      'sources': [
        'minimal_shell.cc',
        'minimal_shell.h',
      ],
    },
  ],
}
