# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target for instrumented dynamic
# libraries and describes standard build action for most of the libraries.

{
  'type': 'none',
  'actions': [
    {
      'action_name': '<(_target_name)',
      'inputs': [
        'download_build_install.py',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/instrumented_libraries/asan/<(_target_name).txt',
      ],
      'action': ['./download_build_install.py', '-i', '<(PRODUCT_DIR)', '-l', '<(_target_name)', '-m', '<(INTERMEDIATE_DIR)', '-s', 'asan'],
    },
  ],
}
