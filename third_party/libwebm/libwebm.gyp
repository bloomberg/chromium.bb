# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'libwebm.gypi',
  ],
  'targets': [
    {
      'target_name': 'libwebm',
      'type': 'static_library',
      'sources': [
        '<@(libwebm_sources)'
      ]
    },  # target libwebm
  ]
}
