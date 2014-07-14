# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'enable_wexit_time_destructors': 1,
    'chromium_code': 1
  },
  'targets': [
    {
      # GN version: //webkit/common:common",
      'target_name': 'webkit_common',
      'type': '<(component)',
      'defines': [
        'WEBKIT_COMMON_IMPLEMENTATION',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../url/url.gyp:url_lib',
      ],
      'sources': [
        'data_element.cc',
        'data_element.h',
        'webkit_common_export.h',
      ],
    },
  ],
}
