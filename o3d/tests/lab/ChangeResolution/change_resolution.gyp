# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ChangeResolution',
      'type': 'executable',
      'defines': ['_WIN32_WINNT=0x0501'],  # for ChangeDisplaySettings
      'sources': [
        'change_resolution.cpp',
      ],
    },
  ],
}
