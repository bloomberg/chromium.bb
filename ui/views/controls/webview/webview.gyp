# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'webview',
      'type': '<(component)',
      'dependencies': [
        '../../../../base/base.gyp:base',
        '../../../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../../../skia/skia.gyp:skia',
        '../../../../content/content.gyp:content',
        '../../../ui.gyp:ui',
        '../../views.gyp:views',
      ],
      'sources': [
        'webview.cc',
        'webview.h',
      ],
    },
  ],
}
