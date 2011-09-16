# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets' : [
        {
          'target_name' : 'mach_override',
          'type': 'static_library',
          'toolsets': ['host', 'target'],
          'sources': [
            'mach_override.c',
            'mach_override.h',
          ],
        },
      ],
    }],
  ],
}
