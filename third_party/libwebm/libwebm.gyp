# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libwebm',
      'type': 'static_library',
      'sources': [
        'source/mkvmuxer.cpp',
        'source/mkvmuxerutil.cpp',
        'source/mkvwriter.cpp',
      ],
    },  # target libwebm
  ]
}
