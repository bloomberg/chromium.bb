# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'webkit_child',
      'type': '<(component)',
      'defines': [
        'WEBKIT_CHILD_IMPLEMENTATION',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
      ],
      'sources': [
        'resource_loader_bridge.cc',
        'resource_loader_bridge.h',
        'webkit_child_export.h',
      ],
    },
  ],
}
