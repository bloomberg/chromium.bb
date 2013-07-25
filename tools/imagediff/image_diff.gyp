# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets' : [
    {
      'target_name': 'image_diff',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../webkit/support/webkit_support.gyp:webkit_support_gfx',
      ],
      'sources': [
        'image_diff.cc',
      ],
      'conditions': [
       ['OS=="android" and android_webview_build==0', {
         # The Chromium Android port will compare images on host rather
         # than target (a device or emulator) for performance reasons.
         'toolsets': ['host'],
       }],
       ['OS=="android" and android_webview_build==1', {
         'type': 'none',
       }],
      ],
    },
  ],
}
