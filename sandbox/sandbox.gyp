# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'conditions': [
    [ 'OS=="win"', {
      'includes': [
        'sandbox_win.gypi',
      ],
    }],
    [ 'OS=="linux"', {
      'includes': [
        'sandbox_linux.gypi',
      ],
    }],
    [ 'OS!="win" and OS!="mac" and OS!="linux"', {
      # We need a 'default' to accomodate the "sandbox" target, for instance
      # on Android.
      'targets': [
        {
          'target_name': 'sandbox',
          'type': 'none',
        }
       ]
    }],
  ],
}
