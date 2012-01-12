# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    'character_composer_unittest.cc',
    'input_method_ibus_unittest.cc',
  ],
  'conditions': [
    ['use_aura==0 or use_x11==0', {
      'sources!': [
        'character_composer_unittest.cc',
        'input_method_ibus_unittest.cc',
      ],
    }],
  ],
}
