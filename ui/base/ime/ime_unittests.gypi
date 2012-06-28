# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'ime_test_files': [
      'character_composer_unittest.cc',
      'input_method_ibus_unittest.cc',
    ],
  },
  'sources': [
    '<@(ime_test_files)',
  ],
  'conditions': [
    ['use_aura==0 or use_x11==0 or chromeos==0', {
      'sources!': [
        '<@(ime_test_files)',
      ],
    }],
  ],
}
