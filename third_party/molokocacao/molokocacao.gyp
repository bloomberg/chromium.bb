# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'molokocacao',
      'type': 'static_library',
      'dependencies': [
        '../../ui/ui.gyp:ui_cocoa_third_party_toolkits',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
        ],
      },
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'NSBezierPath+MCAdditions.h',
        'NSBezierPath+MCAdditions.m',
      ],
    },
  ],
}
