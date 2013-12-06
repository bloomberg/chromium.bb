# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'mozilla',
      'type': 'static_library',
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
        ],
      },
      'dependencies': [
        '../../url/url.gyp:url_lib',
      ],
      'sources': [
        'ComplexTextInputPanel.h',
        'ComplexTextInputPanel.mm',
        'NSPasteboard+Utils.h',
        'NSPasteboard+Utils.mm',
        'NSScreen+Utils.h',
        'NSScreen+Utils.m',
        'NSString+Utils.h',
        'NSString+Utils.mm',
        'NSURL+Utils.h',
        'NSURL+Utils.m',
        'NSWorkspace+Utils.h',
        'NSWorkspace+Utils.m',
      ],
    },
  ],
}
