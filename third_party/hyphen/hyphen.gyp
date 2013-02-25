# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'hyphen',
      'type': 'static_library',
      'include_dirs': [
        '.',
      ],
      'sources': [
        'hnjalloc.c',
        'hnjalloc.h',
        'hyphen.h',
        'hyphen.c',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      # TODO(jschuh): http://crbug.com/167187
      'msvs_disabled_warnings': [
        4018,
        4267,
      ],
    },
  ],
}
