# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets' : [
        {
          'target_name' : 'icon_family',
          'type': 'static_library',
          'sources': [
            'IconFamily.h',
            'IconFamily.m',
            'NSString+CarbonFSRefCreation.h',
            'NSString+CarbonFSRefCreation.m',
          ],
          'defines': [
            'DISABLE_CUSTOM_ICON'
          ],
        },
      ],
    }],
  ],
}
