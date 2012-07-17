# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hyphen',
      'type': '<(library)',
      'include_dirs': [
        '.',
      ],
      'defines': [
        'HYPHEN_CHROME_CLIENT',
      ],
      'sources': [
        'hnjalloc.c',
        'hnjalloc.h',
        'hyphen.h',
        'hyphen.c',
      ],
      'direct_dependent_settings': {
        'defines': [
          'HYPHEN_CHROME_CLIENT',
        ],
        'include_dirs': [
          '.',
        ],
      },
    },
  ],
}
