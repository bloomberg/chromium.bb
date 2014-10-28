# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'omaha_extractor',
      'type': 'static_library',
      'sources': [
        'src/omaha/base/extractor.cc',
        'src/omaha/base/extractor.h',
      ],
      'include_dirs': [
        '../..',
        'src',
      ],
    },
  ],
}
