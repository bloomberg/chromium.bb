# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'shell',
      'type': 'static_library',
      'dependencies': [
        '../aura/aura.gyp:aura',
        '../views/views.gyp:views',
        '../../skia/skia.gyp:skia',
      ],
      'sources': [
        'minimal_shell.cc',
        'minimal_shell.h',
      ],
    },
  ],
}
